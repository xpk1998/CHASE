// Copyright (c) Mysten Labs, Inc.
// SPDX-License-Identifier: Apache-2.0
//
// Tusk: asynchronous DAG-based BFT consensus from
// "Narwhal and Tusk: A DAG-based Mempool and Efficient BFT Consensus".
// This implementation reuses the Narwhal DAG ordering logic with a fixed
// leader schedule (no Bullshark reputation-based leader swapping).

use crate::consensus::{ConsensusState, Dag, LeaderSchedule};
use crate::metrics::ConsensusMetrics;
use crate::{utils, ConsensusError, Outcome};
use config::{Committee, Stake};
use fastcrypto::hash::Hash;
use std::sync::Arc;
use storage::ConsensusStore;
use tokio::time::Instant;
use tracing::{debug, error_span};
use types::{Certificate, CertificateAPI, CommittedSubDag, HeaderAPI, ReputationScores, Round};

#[cfg(test)]
#[path = "tests/tusk_tests.rs"]
pub mod tusk_tests;

/// Tusk consensus engine operating over a Narwhal DAG.
pub struct Tusk {
    pub committee: Committee,
    pub store: Arc<ConsensusStore>,
    pub max_inserted_certificate_round: Round,
    pub metrics: Arc<ConsensusMetrics>,
    pub last_successful_leader_election_timestamp: Instant,
    pub leader_schedule: LeaderSchedule,
}

impl Tusk {
    pub fn new(
        committee: Committee,
        store: Arc<ConsensusStore>,
        metrics: Arc<ConsensusMetrics>,
        leader_schedule: LeaderSchedule,
    ) -> Self {
        Self {
            committee,
            store,
            last_successful_leader_election_timestamp: Instant::now(),
            max_inserted_certificate_round: 0,
            metrics,
            leader_schedule,
        }
    }

    pub fn process_certificate(
        &mut self,
        state: &mut ConsensusState,
        certificate: Certificate,
    ) -> Result<(Outcome, Vec<CommittedSubDag>), ConsensusError> {
        debug!("Processing {:?}", certificate);
        let round = certificate.round();

        if !state.try_insert(&certificate)? {
            return Ok((Outcome::CertificateBelowCommitRound, vec![]));
        }

        self.report_leader_on_time_metrics(round, state);

        let r = round - 1;
        if r % 2 != 0 || r < 2 {
            return Ok((Outcome::NoLeaderElectedForOddRound, Vec::new()));
        }

        let leader_round = r;
        if leader_round <= state.last_round.committed_round {
            return Ok((Outcome::LeaderBelowCommitRound, Vec::new()));
        }

        let leader = match self
            .leader_schedule
            .leader_certificate(leader_round, &state.dag)
        {
            (_leader_authority, Some(certificate)) => certificate,
            (_leader_authority, None) => {
                return Ok((Outcome::LeaderNotFound, Vec::new()));
            }
        };

        let stake: Stake = state
            .dag
            .get(&round)
            .expect("We should have the whole history by now")
            .values()
            .filter(|(_, x)| x.header().parents().contains(&leader.digest()))
            .map(|(_, x)| self.committee.stake_by_id(x.origin()))
            .sum();

        if stake < self.committee.validity_threshold() {
            debug!("Leader {:?} does not have enough support", leader);
            return Ok((Outcome::NotEnoughSupportForLeader, Vec::new()));
        }

        debug!("Leader {:?} has enough support", leader);
        let mut committed_sub_dags = Vec::new();
        let mut total_committed_certificates = 0;

        for leader in self.order_leaders(leader, state).iter().rev() {
            let sub_dag_index = state.next_sub_dag_index();
            let _span = error_span!("tusk_process_sub_dag", sub_dag_index);

            let mut min_round = leader.round();
            let mut sequence = Vec::new();

            for x in utils::order_dag(leader, state) {
                state.update(&x);
                min_round = min_round.min(x.round());
                sequence.push(x);
            }
            debug!(min_round, "Subdag has {} certificates", sequence.len());

            total_committed_certificates += sequence.len();

            let reputation_score = ReputationScores::new(&self.committee);

            let sub_dag = CommittedSubDag::new(
                sequence,
                leader.clone(),
                sub_dag_index,
                reputation_score,
                state.last_committed_sub_dag.as_ref(),
            );

            self.store
                .write_consensus_state(&state.last_committed, &sub_dag)?;

            state.last_committed_sub_dag = Some(sub_dag.clone());
            committed_sub_dags.push(sub_dag);
        }

        let elapsed = self.last_successful_leader_election_timestamp.elapsed();
        self.metrics
            .commit_rounds_latency
            .observe(elapsed.as_secs_f64());
        self.last_successful_leader_election_timestamp = Instant::now();

        self.metrics
            .leader_commits
            .with_label_values(&["strong"])
            .inc();
        self.metrics
            .leader_commits
            .with_label_values(&["weak"])
            .inc_by(committed_sub_dags.len() as u64 - 1);

        for (name, round) in &state.last_committed {
            debug!("Latest commit of {}: Round {}", name, round);
        }

        self.metrics
            .committed_certificates
            .report(total_committed_certificates as u64);

        Ok((Outcome::Commit, committed_sub_dags))
    }

    pub fn order_leaders(&self, leader: &Certificate, state: &ConsensusState) -> Vec<Certificate> {
        let mut to_commit = vec![leader.clone()];
        let mut leader = leader;
        assert_eq!(leader.round() % 2, 0);
        for r in (state.last_round.committed_round + 2..=leader.round() - 2)
            .rev()
            .step_by(2)
        {
            let (prev_leader, authority) =
                match self.leader_schedule.leader_certificate(r, &state.dag) {
                    (authority, Some(x)) => (x, authority),
                    (authority, None) => {
                        self.metrics
                            .leader_election
                            .with_label_values(&["not_found", authority.hostname()])
                            .inc();
                        continue;
                    }
                };

            if self.linked(leader, prev_leader, &state.dag) {
                to_commit.push(prev_leader.clone());
                leader = prev_leader;
            } else {
                self.metrics
                    .leader_election
                    .with_label_values(&["no_path", authority.hostname()])
                    .inc();
            }
        }

        let committee = self.committee.clone();
        let metrics = self.metrics.clone();
        to_commit.iter().for_each(|certificate| {
            let authority = committee.authority(&certificate.origin()).unwrap();
            metrics
                .leader_election
                .with_label_values(&["committed", authority.hostname()])
                .inc();
        });

        to_commit
    }

    fn linked(&self, leader: &Certificate, prev_leader: &Certificate, dag: &Dag) -> bool {
        let mut parents = vec![leader];
        for r in (prev_leader.round()..leader.round()).rev() {
            parents = dag
                .get(&r)
                .expect("We should have the whole history by now")
                .values()
                .filter(|(digest, _)| {
                    parents
                        .iter()
                        .any(|x| x.header().parents().contains(digest))
                })
                .map(|(_, certificate)| certificate)
                .collect();
        }
        parents.contains(&prev_leader)
    }

    fn report_leader_on_time_metrics(&mut self, certificate_round: Round, state: &ConsensusState) {
        if certificate_round > self.max_inserted_certificate_round
            && certificate_round % 2 == 0
            && certificate_round > 2
        {
            let previous_leader_round = certificate_round - 2;
            let authority = self.leader_schedule.leader(previous_leader_round);

            if state.last_round.committed_round < previous_leader_round {
                self.metrics
                    .leader_commit_accuracy
                    .with_label_values(&["miss", authority.hostname()])
                    .inc();
            } else {
                self.metrics
                    .leader_commit_accuracy
                    .with_label_values(&["hit", authority.hostname()])
                    .inc();
            }
        }

        self.max_inserted_certificate_round =
            self.max_inserted_certificate_round.max(certificate_round);
    }
}
