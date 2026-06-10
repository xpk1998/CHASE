// Copyright (c) Mysten Labs, Inc.
// SPDX-License-Identifier: Apache-2.0

use crate::consensus::{ConsensusState, LeaderSchedule, LeaderSwapTable};
use crate::tusk::Tusk;
use crate::{metrics::ConsensusMetrics, Outcome};
use config::Committee;
use fastcrypto::hash::Hash;
use prometheus::Registry;
use std::collections::BTreeSet;
use std::sync::Arc;
use storage::ConsensusStore;
use test_utils::{latest_protocol_version, CommitteeFixture};
use types::Certificate;

#[tokio::test]
async fn tusk_processes_certificate_without_commit_on_odd_round() {
    let fixture = CommitteeFixture::builder().build();
    let committee = fixture.committee();
    let registry = Registry::new();
    let metrics = Arc::new(ConsensusMetrics::new(&registry));
    let store = Arc::new(ConsensusStore::new_for_tests());

    let leader_schedule = LeaderSchedule::new(committee.clone(), LeaderSwapTable::default());
    let mut state = ConsensusState::new(metrics.clone(), 50);
    let mut tusk = Tusk::new(committee, store, metrics, leader_schedule);

    let genesis = Certificate::genesis(&fixture.committee())
        .iter()
        .map(|x| x.digest())
        .collect::<BTreeSet<_>>();
    let authority = fixture.authorities().next().unwrap().id();
    let (_, cert) = test_utils::mock_certificate(
        &fixture.committee(),
        &latest_protocol_version(),
        authority,
        1,
        genesis,
    );

    let (outcome, sub_dags) = tusk
        .process_certificate(&mut state, cert)
        .expect("should not error");
    assert_eq!(outcome, Outcome::NoLeaderElectedForOddRound);
    assert!(sub_dags.is_empty());
}
