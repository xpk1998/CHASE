// Copyright (c) Mysten Labs, Inc.
// SPDX-License-Identifier: Apache-2.0

use crate::bullshark::Bullshark;
use crate::consensus::ConsensusState;
use crate::tusk::Tusk;
use crate::{ConsensusError, Outcome};
use types::{Certificate, CommittedSubDag};

/// Pluggable DAG-based ordering protocol used by the consensus core.
pub enum ConsensusProtocol {
    Bullshark(Bullshark),
    Tusk(Tusk),
}

impl ConsensusProtocol {
    pub fn process_certificate(
        &mut self,
        state: &mut ConsensusState,
        certificate: Certificate,
    ) -> Result<(Outcome, Vec<CommittedSubDag>), ConsensusError> {
        match self {
            Self::Bullshark(engine) => engine.process_certificate(state, certificate),
            Self::Tusk(engine) => engine.process_certificate(state, certificate),
        }
    }
}
