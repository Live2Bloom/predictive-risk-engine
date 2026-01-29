#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

//The central repository for a specific investment type's performance and risk metrics.
typedef struct {
    //simply just the type of investment for this data
    char type_name[20];
    //Dynamic array tracking historical percentage changes.
    float *returns;
    //Tracking current size and allocated memory for the returns array.
    int day_count;
    int capacity;
    //Basic statistical profile of the asset.
    float mean;
    float std_dev;
    //VAR prediction based on historical sampling (Monte Carlo).
    float worst_case;
    //Analytical risk prediction derived from numerical integration (Riemann Sum).
    float worst_case_rieman;
} Portfolio;

//Used during the CSV ingestion phase to map data from the file system to memory.
typedef struct{
    char type[20];
    float value;
} RawData;

//Optimized for 11 buckets to balance collision management and memory footprint
#define TABLE_SIZE 11
//Buffer size for CSV line parsing.
#define SIZE_LINE 256
//Constants utilized for Gaussian distribution and Box-Muller transformations.
#define PI 3.1415927
#define INV_SQRT_2PI (1.0f / sqrtf(PI * 2.0f))
//Global Hash Table for Portfolio Storage
Portfolio *buckets[TABLE_SIZE];

// Maps a string identifier to a specific index in the global buckets array.
// Uses a basic hashing algorithm to ensure uniform distribution.
int hash(char* type);


//Initializes a new Portfolio structure for a specific asset class.
//Allocates initial memory for historical data and sets defaults.
Portfolio* create_bucket(char *type);


//Parses a single string from a CSV source, converting raw text
//into a structured RawData format for processing.
void load(char* line, RawData* data);


//Implements the Box-Muller transform to generate a normally distributed
//synthetic dataset (10,000 samples) based on provided parameters.
float* synth_data_generator(float mean, float deviation);

//Calculates the arithmetic mean of a float array.
float mean(float* data, int count);

//Computes the standard deviation of a dataset to measure volatility.
float stand_dev(float* data, int count, float mean);

//Sorts data and performs a historical simulation to identify the 5th percentile loss.
int compare(const void *a, const void *b);
void analyze(float* data, Portfolio* bucket);

//Calculates the 5% Value at Risk by scanning the Probability Density Function.
//using a Riemann sum approach to find the cumulative density threshold.
void rieman(float* data, float mean, float deviation, Portfolio* address);


//Formats the analytical results and pipes them to stdout for integration with the Python dashboard.
int send2python(Portfolio* ptr, char* user_query);



/**
 * Main Execution Loop
 * Handles file I/O, memory management, and orchestration of the analysis pipeline.
 * * Usage: ./risk_engine <csv_file> <investment_type>
 */
int main(int argc, char* argv[]){
    //Variable Initialization
    float *temp_data = malloc(sizeof(float) * 10000);
    FILE *input_data;
    RawData current_entry;
    char buffer[SIZE_LINE];
    Portfolio *type_bucket;
    float average;
    float sdev;
    int index;
    Portfolio *ptr;
    int test;

    // Phase 1: Argument and File Validation
    if (argc != 3){
        return 1; // Incorrect usage
    }
    if ((input_data = fopen(argv[1], "r")) == NULL){
        return 1; // File access error
    }
    srand(time(NULL));
    // Phase 2: Data Ingestion and Dynamic Memory Scaling
    while (fgets(buffer, SIZE_LINE, input_data) != NULL) {
        load(buffer, &current_entry);
        index = hash(current_entry.type);
        //Initialization of hash table buckets
        if (buckets[index] == NULL) {
            buckets[index] = create_bucket(current_entry.type);
            if (buckets[index] == NULL) return 1;
        }

        // Dynamic Array Resizing to avoid memory leak or segmentation fault.
        if (buckets[index]->day_count >= buckets[index]->capacity) {
            int new_cap = buckets[index]->capacity * 2;
            float *new_ptr = realloc(buckets[index]->returns, new_cap * sizeof(float));
            if (new_ptr == NULL) return 1;

            buckets[index]->returns = new_ptr;
            buckets[index]->capacity = new_cap;
        }

        buckets[index]->returns[buckets[index]->day_count] = current_entry.value;
        buckets[index]->day_count++;
    }
    // Phase 3: Target Data Retrieval
    index = hash(argv[2]);
    if (buckets[index] == NULL) {
        return 3; // Target investment type not found in dataset
    }
    // Phase 4: Statistical Analysis
    average = mean(buckets[index]->returns, buckets[index]->day_count);
    buckets[index]->mean = average;
    sdev = stand_dev(buckets[index]->returns, buckets[index]->day_count, average);
    if (sdev == 0.0){
        return 2;
    }
    buckets[index]->std_dev = sdev;
    // Phase 5: Predictive Modeling (Monte Carlo & Riemann)
    temp_data = synth_data_generator(average, sdev);
    analyze(temp_data, buckets[index]);
    rieman(buckets[index]->returns, average, sdev, buckets[index]);
    // Phase 6: Cross-Platform Communication
    test = send2python(buckets[index], argv[2]);
    if (test == 1){
        return 3;
    }
    // Cleanup
    free(temp_data);
    return 0;
}


/**
 * Parses a single line from the CSV file and populates a RawData structure.
 * * @param line: The raw string read from the file.
 * @param data: Pointer to the RawData struct where parsed info will be stored.
 */
void load(char* line, RawData* data ){
    // Isolate the first token (Investment Type)
    char *type = strtok(line, ",");
    if (type == NULL) return;

    //DATA INTEGRITY FIX:
     //Remove trailing newline or carriage return characters (\r\n).
     //If these remain, the hash() function will treat "S&P500\n"
     //differently than "S&P500", breaking bucket lookups.
    type[strcspn(type, "\r\n")] = 0;

    // Isolate the second token (Return Value)
    char *return_str = strtok(NULL, ",");
    if (return_str == NULL) return;
    // Transfer the data to the struct
    strcpy(data->type, type);
    data->value = atof(return_str);
}


/**
 * Generates a consistent hash index for a given asset string.
 * This ensures that specific investment types are always routed
 * to the same portfolio bucket.
 * * @param type: The raw string identifier for the investment.
 * @return: An integer index between 0 and (TABLE_SIZE - 1).
 */
int hash(char* type){
    //Initialize with a large prime to provide a head start for distribution
    long total = 5381;
    char upperchar;
    int charint;

    for (int i = 0; type[i] != '\0'; i++){
        //Normalize to uppercase for case-insensitivity
        upperchar = toupper(type[i]);
        //Calculate numeric weight based on alphabet position
        charint = (int)upperchar-'A';
        //Offset adjustment to ensure 'A' (0) still contributes to the hash
        if (charint == 0){
            charint = 3;
        }
        //Multiplying by 33 (shifting and adding) helps spread the
        //bits of the previous characters across the total.
        total = (total * 33)+charint;
    }
    //Wrap the result within the bounds of the hash table
    int index = total % TABLE_SIZE;
    if (index < 0){
        index = index * -1;
    }
    return index;
}


/**
 * Allocates and initializes a new Portfolio bucket.
 * * @param type: The string label for the investment type.
 * @return: A pointer to the initialized Portfolio, or NULL on failure.
 */
Portfolio* create_bucket(char *type){
    // Step 1: Allocate the primary structure
    Portfolio* bucket = malloc(sizeof(Portfolio));
    // Safety check: ensure the OS granted the memory request
    if (bucket == NULL){
        free(bucket);
        return NULL;
    }
    // Step 2: Allocate the dynamic array for historical returns
    // Starting with a baseline capacity of 50 entries
    bucket->returns = malloc(sizeof(float)*50);
    // cleanup to avoid memory leak
    if (bucket->returns == NULL){
        free(bucket);
        return NULL;
    }

    // Step 3: Initialize metadata and defaults
    strcpy(bucket->type_name, type);
    bucket->day_count = 0;
    bucket->capacity = 50;

    // Initialize stats to zero to prevent garbage value calculations
    bucket->mean = 0.0f;
    bucket->std_dev = 0.0f;
    bucket->worst_case = 0.0f;
    bucket->worst_case_rieman = 0.0f;

    return bucket;
}

/**
 * Calculates the average historical return for a specific asset.
 * * @param data: Pointer to the array of float return values.
 * @param count: The number of entries (day_count) in the dataset.
 * @return: The arithmetic mean as a float.
 */
float mean(float* data, int count){
    // Safety check: prevent division by zero or processing empty datasets
    if (count == 0){
        return 0;
    }
    //I use 'double' for the accumulator to maintain precision.
    //In MLOps (my study of passion is macheine learning), accumulating thousands of small floats can lead to
    //rounding errors if the accumulator doesn't have enough significant digits.
    double total_value = 0;
    // Summing the historical performance data
    for (int i = 0; i < count; i++){
        //add the return float to total_value
        total_value += data[i];
    }
    // Calculate final mean and return as float
    float mean = total_value/count;
    return mean;
}

/**
 * Computes the sample standard deviation for the portfolio returns.
 * * @param data: Pointer to the array of historical returns.
 * @param count: Number of data points.
 * @param mean: The pre-calculated average return.
 * @return: The standard deviation (volatility) as a float.
 */
float stand_dev(float* data, int count, float mean){
    // Base Case: Standard deviation requires at least two points to measure spread
    if (count < 2){
        return 0.0;
    }
    //ACCUMULATION STRATEGY:
    //We use a high-precision double to store the "Sum of Squares."
    //Squaring differences ensures that negative deviations (losses)
    //don't cancel out positive deviations (gains).
    double total_value = 0;
    //create a for loop that loops until it hits day_count
    for (int i =0; i < count; i++){
        //subtract the mean from the return and square it in case of negitive value to add variance
        total_value += (data[i]-mean) * (data[i]-mean);
    }

     //Using (count - 1) provides an unbiased estimate of the
     //population variance when working with sample data.
    float dev_from_variance = sqrt(total_value/(count-1));

    return dev_from_variance;
}

/**
 * Generates a synthetic dataset of 10,000 points based on asset statistics.
 * Uses the Box-Muller transform to produce a normal distribution.
 * * @param mean: The target average for the distribution.
 * @param deviation: The target volatility for the distribution.
 * @return: A pointer to a heap-allocated array of 10,000 floats.
 */
float* synth_data_generator(float mean, float deviation){
    // Allocation for the synthetic 10k sample set
    float* generated_returns = malloc(sizeof(float)*10000);
    if (generated_returns == NULL){
        return NULL;
    }
    //initialize variables for for loop
    float u1;
    float u2;
    float transform_u1;
    float transform_u2;
    float gravity;

    //We iterate 5,000 times, generating two points per iteration
    //to populate the full 10,000-node array.
    for (int i = 0; i < 10000-1; i+=2){
        // Generate uniform random numbers in the range (0, 1]
        u1 = (rand()+1.0)/(RAND_MAX+1.0);
        u2 = (rand()+1.0)/(RAND_MAX+1.0);
        /* * BOX-MULLER TRANSFORM:
         * 'gravity' calculates the magnitude of the offset from the mean.
         */
        gravity = sqrtf(-2*logf(u1));
        // Project magnitude onto the Z-axis using trigonometric oscillation
        transform_u1 = (gravity*(cosf((PI*2)*u2))*deviation)+mean;
        transform_u2 = (gravity*(sinf((PI*2)*u2))*deviation)+mean;
        //add both variables into the array
        generated_returns[i] = transform_u1;
        generated_returns[i+1] = transform_u2;
    }

    return generated_returns;
}

/**
 * Comparison utility for qsort.
 * Ensures the synthetic dataset is ordered from greatest loss to greatest gain.
 */
int compare(const void *a, const void *b){
    const float fa = *(const float*)a;
    const float fb = *(const float*)b;
    if (fa < fb){
        return -1;
    }
    else if (fa > fb){
        return 1;
    }
    else{
        return 1;
    }

}
/**
 * Sorts the synthetic dataset and extracts the 5% Value-at-Risk (VaR).
 * * @param data: The 10,000-point synthetic array.
 * @param bucket: The Portfolio structure to update with the result.
 */
void analyze(float* data, Portfolio* bucket){

    //Using qsort (O(n log n)) ensures the analysis remains efficient
    //even as we scale the simulation size.
    qsort(data, 10000, sizeof(float), &compare);
    //Index 499 represents the 5th percentile of 10,000 samples.
    //This is our "Monte Carlo" Worst Case Scenario.
    float worst_case = data[499];
     // Update the portfolio metadata with the calculated risk profile
    bucket->worst_case = worst_case;
}

/**
 * Calculates the 5% Value-at-Risk using numerical Riemann integration.
 * This serves as a deterministic check against the Monte Carlo simulation.
 * * @param data: The historical returns (used for reference).
 * @param mean: The calculated average return.
 * @param deviation: The calculated volatility.
 * @param address: The Portfolio structure to update with the Riemann result.
 */
void rieman(float* data, float mean, float deviation, Portfolio* adress){
    // Cumulative area accumulator (target is 0.05 or 5%)
    float bucket = 0;
    // Start scanning from the extreme left tail of the bell curve.
    float start = mean-(5*deviation);
    float step_size = 0.0001;
    //initialize variables for loop
    float height;
    float x;
    float z;
    //Continues until the 'bucket' (area) reaches 0.05.
    for (x = start; bucket < 0.05; x+=step_size){
        // Calculate the Z-Score (distance from mean in standard deviations)
        z = (x-mean)/deviation;

        /* * GAUSSIAN HEIGHT CALCULATION:
         * 1. (z*z) squares the distance to remove negative signs.
         * 2. expf(-0.5 * z^2) creates the characteristic bell shape.
         * 3. INV_SQRT_2PI / deviation normalizes the total area to 1.0.
         */
        height = INV_SQRT_2PI/deviation*(expf(-0.5*(z*z)));

        // Accumulate the area of the current rectangle (Height * Base)
        bucket += height*step_size;
    }
    //update the buckets worst_case_rieman
    adress->worst_case_rieman = x;
}

/**
 * Transmits processed portfolio metrics to the Python interface.
 * Normalizes risk data and formats it for cross-process communication.
 * * @param ptr: Pointer to the analyzed Portfolio bucket.
 * @param user_query: The specific asset name requested by the user.
 * @return: 0 on success, 1 on failure.
 */
int send2python(Portfolio* ptr, char* user_query){
    //send the data to python
    if (ptr == NULL){
        return 1;
    }
    float min;
    float max;

    //Identify the range between the simulation and the analytical calculation.
    if (ptr->worst_case < ptr->worst_case_rieman){
        min = ptr->worst_case;
        max = ptr->worst_case_rieman;
    }
    else{
        min = ptr->worst_case_rieman;
        max = ptr->worst_case;

    }
    // Normalization parameters
    float floor = 0.0;
    float cap = 1;
    // Converts raw return values into human-readable percentages.
    float min_percentage = ((min-floor)/(cap-floor))*100;
    float max_percentage = ((max-floor)/(cap-floor))*100;
    //Inverts the standard deviation (volatility) to create a 0-100 score.
    float score = ptr->std_dev;
    float stability = 100-(((score-floor)/(cap-floor))*100);
    /* * CROSS-LANGUAGE BRIDGE:
     * Data is piped to Python via stdout in a structured CSV format.
     * Format: Type, Mean, Stability, Min_VaR, Max_VaR
     */
    printf("%s,%.4f,%.4f,%.4f,%.4f\n", ptr->type_name, ptr->mean, stability, min_percentage, max_percentage);
    return 0;
}
