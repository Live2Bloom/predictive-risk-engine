from flask import Flask, flash, redirect, render_template, request, jsonify
import subprocess
import os

app = Flask(__name__)

@app.route("/", methods=["GET"])
def index():
    """
    UI INITIALIZATION
    Defines the initial state of the analytical dashboard.
    The 'assets' list represents the available investment types in the C-Engine.
    """
    assets = ["EQUITY", "CRYPTO", "BOND", "COMMODITY", "FOREX"]
    return render_template("index.html", assets=assets)


@app.route("/result", methods=["GET", "POST"])
def response():
    """
    ASYNC REQUEST HANDLER
    Orchestrates the lifecycle of a single simulation request.
    """
    if request.method == "POST":
        # DATA EXTRACTION: Retrieving the 'Asset Type' metadata from the multipart form.
        type = request.form.get("investment_type")

        # FILE BUFFERING: Capturing the uploaded CSV packet from the request stream.
        data = request.files["file_input_name"]

        # PERSISTENCE: Saving the data to a localized buffer for the C-Engine to access via path.
        os.makedirs("uploads", exist_ok=True)
        data.save("uploads/returns.csv")

        try:
            # INTER-PROCESS COMMUNICATION (IPC): Passing control to the C-Engine bridge.
            inv_type, mean, score, wcmin, wcmax = engine("uploads/returns.csv", type)

            # DATA FORMATTING: Preparing raw numerical outputs for UI-friendly string representation.
            wc_min = f"{wcmin}%"
            wc_max = f"{wcmax}%"
            stability = f"{score}%"

        except NameError:
            # VALIDATION ERROR: Specific handling for non-existent asset categories.
            return render_template("index.html", error="That asset doesn't exist!")
        except Exception as e:
            # SYSTEM ERROR BOUNDARY: Captures and logs pipeline failures (C-crash, FileIO, etc.)
            print(f"Bridge Error: {e}")
            return jsonify({"error": str(e)}), 400

        # SERIALIZATION: Returning the calculated insights to the JS fetch() callback as JSON.
        return jsonify({
            "type": inv_type,
            "mean": mean,
            "stability": stability,
            "min": wc_min,
            "max": wc_max,
        })
    else:
        # FALLBACK: Renders index for standard GET requests.
        return render_template("index.html")


def engine(data, user_query):
    """
    CROSS-STACK BRIDGE
    This is the core architectural link between Python (Web) and C (Math).
    """
    # SUBPROCESS EXECUTION: Running the compiled C binary as a separate system process.
    # Passing the CSV path and Asset Type as command-line arguments.
    result = subprocess.run(["./finance_engine", data, user_query], capture_output=True, text=True)

    # EXIT CODE ANALYSIS: Handling custom error signals defined in the C source code.
    if result.returncode == 1:
        raise FileNotFoundError("CSV File not found or empty.")
    elif result.returncode == 2:
        raise ValueError("Math error: Not enough data points to calculate risk.")
    elif result.returncode == 3:
        raise NameError("Investment type not found in database.")

    # STDOUT PARSING: Capturing the raw string output from the C 'printf' stream.
    c_data = result.stdout
    str_data = c_data.strip()

    # DATA UNPACKING: Splitting the CSV-formatted string returned by C into individual variables.
    try:
        inv_type, mean, stability, wc_min, wc_max = str_data.split(sep=",")
        return inv_type, mean, stability, wc_min, wc_max
    except (ValueError, TypeError):
        # FAIL-SAFE: Returns zero-state data if the C-Engine output is malformed.
        print("Could Not Retreive output data from engine.")
        return ("N/A", "0", "0", "0", "0")

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 5000))
    app.run(host='0.0.0.0', port=port)
