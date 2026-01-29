/**
 * ARCHITECTURAL ANCHORS
 * Selecting DOM elements to serve as the 'State Controllers' for the UI.
 */
const submit = document.getElementById('runsimButton');
const form = document.getElementById('formfordata');
const progress_bar = document.getElementById('loading-bar');
const terminal = document.getElementById('terminal-text');
const mean = document.getElementById('mean-overview');
const stability = document.getElementById('stability-overview');
const worst_case = document.getElementById('worstcase-overview');

/**
 * SIMULATION EVENT LISTENER
 * Triggers the data lifecycle on user interaction.
 */
submit.addEventListener('click', function(e){
    // Prevents standard form submission/page reload to maintain 'Single Page Application' (SPA) feel.
    e.preventDefault();

    // UI FEEDBACK: Activating the progress bar to signal the start of a background process.
    const inner_bar = progress_bar.querySelector('.progress-bar');
    progress_bar.classList.remove('d-none');

    // ANIMATION LOGIC: Applying the linear transition for the simulated load time.
    inner_bar.style.width = '100%';
    inner_bar.classList.add('probar_trans');

    /**
     * DATA PACKAGING
     * Encapsulating the CSV file and metadata into a FormData object for binary transmission.
     */
    const formdata = new FormData(form);

    /**
     * ASYNCHRONOUS BRIDGE (Fetch API)
     * POSTing the payload to the Flask /result endpoint.
     */
    fetch('/result', {
        method: 'POST',
        body: formdata
    })
    // DATA UNPACKING: Converting the raw HTTP response into a usable JSON object.
    .then(response => response.json())
    .then(data => {
        // UI INJECTION: Updating the Mean Return card with server-calculated data.
        mean.innerText = data.mean;

        /**
         * DATA NORMALIZATION (Cleaning)
         * Stripping symbols and converting the String response to a Float for logical comparison.
         */
        const numericStability = parseFloat(data.stability.replace('%', ''));

        // RE-INITIALIZATION: Clearing previous 'Mood' classes to prevent style bleeding.
        stability.classList.remove('status-safe', 'status-warning', 'status-danger');

        /**
         * CONDITIONAL RENDERING (The Gauge)
         * Assigning visual 'Mood' classes based on volatility thresholds.
         */
        if (numericStability < 70) {
            stability.classList.add('status-danger'); // High Volatility
        } else if (numericStability > 90) {
            stability.classList.add('status-safe');   // High Stability
        } else {
            stability.classList.add('status-warning'); // Moderate Risk
        }

        // Final Text Update for Stability Card
        stability.innerText = data.stability;
        worst_case.innerText = `${data.min} - ${data.max}`;

        /**
         * TERMINAL LOG SIMULATION
         * Creating a sequence of status updates to visualize the internal C-Engine lifecycle.
         */
        let lines = [
            `[SYSTEM]: Initializing Finance Engine...`,
            `[DATA]: Loading CSV packet for ${data.type}...`,
            `[MATH]: Performing Riemann Summation on volatility...`,
            `[MATH]: Standard Deviation identified as ${data.stability}%`,
            `[RESULT]: Mean Return calculated at ${data.mean}`,
            `[BOUNDS]: Worst-case range: [${data.min} to ${data.max}]`,
            `[STATUS]: Simulation Complete. Results Pushed to UI.`
        ];

        // LOG SEQUENCING: Using index-based timeouts to create a real-time 'Typing' effect.
        terminal.innerText = '';
        lines.forEach(function(line, index){
            setTimeout(() => {
                terminal.innerText += line + '\n';

                // CLEANUP: Resetting the progress bar state after the final log entry.
                if (index === lines.length - 1) {
                    progress_bar.classList.add('d-none');
                    inner_bar.style.width = '0%';
                    inner_bar.classList.remove('probar_trans');
                }
            }, index * 500); // 500ms delay between log entries
        })

    })
    // ERROR BOUNDARY: Catching network or server-side failures to prevent UI crashing.
    .catch(error => {
        console.error('Fetch error:', error);
    })
});
