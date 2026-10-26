#ifndef COMMON_OTA_RESTART_GATE_HPP
#define COMMON_OTA_RESTART_GATE_HPP

class OtaRestartGate {
public:
    void recordStagedOutcome(bool stagedOutcome, bool trialRunning) {
        this->stagedRestartPending = stagedOutcome && !trialRunning;
    }

    void requestRestart(bool trialRunning) {
        if (this->stagedRestartPending && !trialRunning) {
            this->requestRestartAfterLifecycleDrain();
        } else {
            this->requestImmediateRestart();
        }
    }

    void requestRestartAfterLifecycleDrain() { this->restartRequested = true; }

    void requestImmediateRestart() { this->immediateRestartRequested = true; }

    void lifecycleResultSubmitted() {
        this->lifecycleSubmitted = true;
        this->tcpDrainSucceeded = false;
    }

    void tcpDrainCompleted(bool succeeded) {
        this->tcpDrainSucceeded = this->lifecycleSubmitted && succeeded;
    }

    bool hasPendingRestart() const {
        return this->immediateRestartRequested || this->restartRequested;
    }

    bool takeRestart() {
        if (!this->immediateRestartRequested &&
            !(this->restartRequested && this->lifecycleSubmitted &&
              this->tcpDrainSucceeded)) {
            return false;
        }

        this->restartRequested = false;
        this->immediateRestartRequested = false;
        this->lifecycleSubmitted = false;
        this->tcpDrainSucceeded = false;
        return true;
    }

private:
    bool restartRequested = false;
    bool immediateRestartRequested = false;
    bool lifecycleSubmitted = false;
    bool tcpDrainSucceeded = false;
    bool stagedRestartPending = false;
};

#endif  // COMMON_OTA_RESTART_GATE_HPP
