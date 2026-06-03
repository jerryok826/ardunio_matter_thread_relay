# Contributing to the Matter-over-Thread Relay Controller

First off, thank you for taking the time to contribute! This project is open-source, and contributions from the community help make this firmware more resilient, feature-rich, and reliable for long-term smart home industrial deployments.

As an embedded development project handling real-time networks (Matter/Thread) and low-level hardware constraints, please review the following guidelines before opening a pull request (PR).

---

## 🛡️ Core Architecture Constraints (Do Not Break)

The firmware in this repository has been heavily optimized to run reliably in an asynchronous, multi-threaded FreeRTOS environment. To ensure stability, all contributions must respect the following architectural constraints:

### 1. Strictly Non-Blocking Execution
- **Never add blocking `delay()` statements anywhere inside the `loop()` or peripheral handlers.** 
- The background Silicon Labs OpenThread engine relies on microsecond-level timing windows. Introducing blocking code starves the network stack, causing dynamic packet drops, frozen status registers, and sudden "No Response" errors in home automation applications.
- Always implement timing intervals using non-blocking tracking variables combined with `millis()` checks.

### 2. Microsecond-Level ISR Isolation
- Keep Hardware Interrupt Service Routines (`pin6_ISR` through `pin9_ISR`) as lightweight as possible. 
- Do not run calculations, serial printing, or display routines inside an active ISR thread. Simply set the volatile state tracking flag (`buttonFlags[i] = true;`) and exit, allowing the main runtime loop to handle physical pin execution.

### 3. Asynchronous Data Coupling & Throttling
- Matter network state updates (`get_onoff()`) occur asynchronously in a low-priority background thread. 
- Any code evaluating or polling smart home network clusters must operate inside a throttled window (configured natively to **50ms**). Unthrottled continuous querying forces memory race conditions, dropping local physical button signals.
- Decouple local physical pins from over-the-air network commands by using the established `shadowMatterStates` tracking array layout to filter out redundant execution loops.

---

## 🔧 Workflow Guidelines

### 1. Code Style & Conventions
- Maintain the **Manual Code Version Tracker** block at the top of the sketch to flag project iterations cleanly during debugging.
- Explicitly pass specific index boundaries (such as `[i]`) across all conditional loops and hardware array mappings. Avoid comparisons that force array decay into raw pointers, which fail silently on compilation check gates.
- Comment any adjustments made to low-level OpenThread system registers (`otInstanceInitSingle()`, etc.) to specify the network safety layout impact.

### 2. Testing Your Submissions
Before committing code or submitting a PR, your build must pass the following benchmark checks on a physical target board:
1. **Compilation Check:** Confirm the sketch compiles with zero warnings or token formatting errors in the Arduino IDE under the target board profile.
2. **Ecosystem Sync Test:** Verify that cycling a local physical push-button immediately mirrors the correct toggle state on your app dashboard within 100ms.
3. **RF Walk-Test Verification:** Ensure the 3-second non-blocking display loop updates the active dBm neighbor data accurately without stalling the local mechanical relay contacts.
4. **Decommission Safety Profile:** Verify that holding the physical user button for 10 full seconds forcefully triggers a clean fabric wipe and triggers a clean hardware system reset (`NVIC_SystemReset()`).

### 3. Opening a Pull Request
- Create a distinct topic branch for your feature or patch. Do not submit changes directly to the `main` branch.
- Structure your commit messages clearly (e.g., `fix: add explicit array indexing boundary to loop validation tracker`).
- Describe the exact hardware setup, display module configuration type (SH1106 or SSD1306), and automation hub you used during testing in your PR summary notes.

---

## 📜 Code of Conduct

By participating in this project, you agree to treat all contributors with respect, provide constructive technical feedback during code reviews, and promote collaborative open-source engineering.

Thank you for helping optimize this intelligent Matter relay node footprint!
