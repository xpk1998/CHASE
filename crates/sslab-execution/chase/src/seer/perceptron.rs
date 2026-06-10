//! Perceptron branch-direction predictor (ported from SeerEVM `core/vm/perceptron.go`).

pub const TAKEN: i32 = 1;
pub const NOT_TAKEN: i32 = -1;
pub const UNCERTAIN: i32 = 0;

const HISTORY_LENGTH_MIN: usize = 10;
const LHR_LENGTH: usize = HISTORY_LENGTH_MIN;
const WT_LENGTH: usize = HISTORY_LENGTH_MIN + 1;
const MAX_WT: i32 = 127;
const MIN_WT: i32 = -128;
const OBSERVER_MODE_THRESHOLD: i32 = 10;
const THETA: f64 = 33.0; // floor(1.93 * LHR_LENGTH + 14) for LHR_LENGTH = 10

#[derive(Debug, Clone)]
pub struct Perceptron {
    history_length: usize,
    weights: Vec<i32>,
    lhr: Vec<i32>,
    last_pred: Vec<i32>,
    enabled: bool,
    round: i32,
    uncertain_num: i32,
}

impl Default for Perceptron {
    fn default() -> Self {
        Self::new()
    }
}

impl Perceptron {
    pub fn new() -> Self {
        Self {
            history_length: 0,
            weights: vec![0; WT_LENGTH],
            lhr: vec![UNCERTAIN; LHR_LENGTH],
            last_pred: Vec::new(),
            enabled: false,
            round: 0,
            uncertain_num: 0,
        }
    }

    fn run(&self) -> i32 {
        let mut prediction = self.weights[0];
        let mut n = LHR_LENGTH;
        let mut i = self.last_pred.len().saturating_sub(1);

        while i < self.last_pred.len() && n > 0 {
            if self.last_pred[i] == UNCERTAIN {
                n = n.saturating_sub(1);
                if i == 0 {
                    break;
                }
                i -= 1;
                continue;
            }
            if self.last_pred[i] == TAKEN {
                prediction += self.weights[n];
            } else {
                prediction -= self.weights[n];
            }
            n = n.saturating_sub(1);
            if i == 0 {
                break;
            }
            i -= 1;
        }

        let mut j = self.lhr.len().saturating_sub(1);
        while n > 0 {
            if self.lhr[j] == UNCERTAIN {
                n = n.saturating_sub(1);
                if j == 0 {
                    break;
                }
                j -= 1;
                continue;
            }
            if self.lhr[j] == TAKEN {
                prediction += self.weights[n];
            } else {
                prediction -= self.weights[n];
            }
            n = n.saturating_sub(1);
            if j == 0 {
                break;
            }
            j -= 1;
        }

        prediction
    }

    pub fn predict(&mut self, train: bool) -> i32 {
        if !train && self.history_length < HISTORY_LENGTH_MIN {
            return UNCERTAIN;
        }

        if self.enabled {
            self.round += 1;
            if self.round % 10 > 0 {
                return UNCERTAIN;
            }
            let prediction = self.run();
            if (prediction as f64).abs() < THETA {
                return UNCERTAIN;
            }
            self.enabled = false;
            self.round = 0;
            return if prediction > 0 { TAKEN } else { NOT_TAKEN };
        }

        let prediction = self.run();
        if (prediction as f64).abs() < THETA {
            self.uncertain_num += 1;
            if self.uncertain_num == OBSERVER_MODE_THRESHOLD {
                self.enabled = true;
                self.uncertain_num = 0;
            }
            UNCERTAIN
        } else if prediction > 0 {
            self.uncertain_num = 0;
            TAKEN
        } else {
            self.uncertain_num = 0;
            NOT_TAKEN
        }
    }

    pub fn update(&mut self, dir: i32, pred_dir: i32) {
        self.history_length += 1;
        if pred_dir == UNCERTAIN || pred_dir != dir {
            if dir == TAKEN {
                self.weights[0] += 1;
            } else if dir == NOT_TAKEN {
                self.weights[0] -= 1;
            }
            for i in 0..LHR_LENGTH {
                if self.lhr[i] == UNCERTAIN {
                    continue;
                }
                if dir == self.lhr[i] {
                    if self.weights[i + 1] < MAX_WT {
                        self.weights[i + 1] += 1;
                    }
                } else if self.weights[i + 1] > MIN_WT {
                    self.weights[i + 1] -= 1;
                }
            }
        }

        self.lhr.push(dir);
        if self.history_length > HISTORY_LENGTH_MIN {
            self.history_length -= 1;
            self.lhr.remove(0);
        }

        if self.last_pred.len() > 1 {
            self.last_pred.remove(0);
        } else {
            self.last_pred.clear();
        }
    }

    pub fn push_last_pred(&mut self, pred: i32) {
        self.last_pred.push(pred);
    }

    pub fn last_prediction(&self) -> Option<i32> {
        self.last_pred.first().copied()
    }

    pub fn has_last_prediction(&self) -> bool {
        !self.last_pred.is_empty()
    }
}

pub fn bool_to_branch_res(dir: bool) -> i32 {
    if dir {
        TAKEN
    } else {
        NOT_TAKEN
    }
}
