#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <math.h>
#include <fcntl.h>
#include <vector>
#include <iterator>

#include "431project.h"

using namespace std;

/*
 * Enter your PSU ID here to select the appropriate dimension scanning order.
 */
#define MY_PSU_ID 910341401

/*
 * Some global variables to track heuristic progress.
 * 
 * Feel free to create more global variables to track progress of your
 * heuristic.
 */

// Global variables 
// vector of dimension indices for my PSU_ID_NUM


// key: dimension index
// value: explored = 1 or not_explored = 0
// Initially all dimesnions are unexplored 

unsigned int dim_index = 0; 
int first_time = 1;
unsigned int currentlyExploringDim = 0;
int currentDimDone = 0;
bool isDSEComplete = false;
unsigned int count_ = 0;
std::map<int, int> explored_dims_map = create_map();
unsigned int STOPPING_THRESHOLD = 75;

/*
 * Given a half-baked configuration containing cache properties, generate
 * latency parameters in configuration string. You will need information about
 * how different cache paramters affect access latency.
 * 
 * Returns a string similar to "1 1 1"
 */
std::string generateCacheLatencyParams(string halfBackedConfig) {

	//string latencySettings;

	//
	//YOUR CODE BEGINS HERE
	//

	// This is a dumb implementation.
	//latencySettings = "1 1 1";
	// create an empty string
    string latencySettings = "";
    // order is dL1lat, iL1lat, uL2lat

    // define latency variables
    int direct_mapped_lat_dL1, direct_mapped_lat_iL1, direct_mapped_lat_uL2;

    // define size variables
    int dL1size, iL1size, uL2size;

    // create a mutable vector of integers, each of which represent the indices of values of dimensions
    string intermediate;
    vector <int> indices;
    stringstream check1(halfBackedConfig);

    while(getline(check1, intermediate, ' '))
    {
        indices.push_back(atoi(intermediate.c_str()));
    }

    dL1size = getdl1size(halfBackedConfig)/1024;
    iL1size = getil1size(halfBackedConfig)/1024;
    uL2size = getl2size(halfBackedConfig)/1024;

    // sanity check...
    // make sure that the cache sizes are in valid ranges
    if(dL1size<2 || dL1size>64)
    	return "0 0 0";

    if(iL1size<2 || iL1size>64)
    	return "0 0 0";

    if(uL2size<32 || uL2size>1024)
    	return "0 0 0";

    direct_mapped_lat_dL1 = log2(dL1size);
    direct_mapped_lat_iL1 = log2(iL1size);
    direct_mapped_lat_uL2 = log2(uL2size);

    if(indices[4]>1)
  	   direct_mapped_lat_dL1+=log2(1<<indices[4]);
    if(indices[6]>1)
  	   direct_mapped_lat_iL1+=log2(1<<indices[6]);
    if(indices[9]>1)
  	   direct_mapped_lat_uL2+=log2(1<<indices[9]);


    //Convert values to indices
    direct_mapped_lat_dL1-=1;
    direct_mapped_lat_iL1-=1;
    direct_mapped_lat_uL2-=5;
    char numstr[21]; // enough to hold all numbers up to 64-bits
    sprintf(numstr, "%d", direct_mapped_lat_dL1);
    latencySettings += string(numstr) + ' ';
    sprintf(numstr, "%d", direct_mapped_lat_iL1);
    latencySettings += string(numstr) + ' ';
    sprintf(numstr, "%d", direct_mapped_lat_uL2);
    latencySettings += string(numstr);
    
    

	//
	//YOUR CODE ENDS HERE
	//

	return latencySettings;
}

/*
 * Returns 1 if configuration is valid, else 0
 */
int validateConfiguration(std::string configuration) {

	// FIXME - YOUR CODE HERE

	// The below is a necessary, but insufficient condition for validating a
	// configuration.
	//return isNumDimConfiguration(configuration);

	// initially the flag is 1, which means that the configuration is valid
    int incorrect_flag = 1;
    int dL1blocksize, iL1blocksize, uL2blocksize;
    int dL1size, iL1size, uL2size;
    int ifqsize;

    // ifqsize comes from width dimension
    ifqsize = 8 * pow(2, extractConfigPararm(configuration,0));

    // calculate blocksizes
    dL1blocksize = 8 * (1 << extractConfigPararm(configuration, 2));
    iL1blocksize = 8 * (1 << extractConfigPararm(configuration, 2));
    uL2blocksize = 16 << extractConfigPararm(configuration, 8);

    // calculate sizes of caches
    // convert to KB
    dL1size = getdl1size(configuration) / 1024;
    iL1size = getil1size(configuration) / 1024;
    uL2size = getl2size(configuration) / 1024;

    // Constriant 1: Length of configuration must be 18
    if(isNumDimConfiguration(configuration) != 1){
        incorrect_flag = 0;
    }
    // Constraint 2: ifqsize = l1blocksize
    if(ifqsize != dL1blocksize){
        incorrect_flag = 0;
    }
    // Constraint 3: dl1blocksize = il1blocksize
    if(dL1blocksize != iL1blocksize){
        incorrect_flag = 0;
    }
    // Constraint 4: uL2blocksize >= 2 * iL1blocksize and uL2blocksize <= 128 bytes
    if(uL2blocksize < 2 * iL1blocksize || uL2blocksize > 128){
        incorrect_flag = 0;
    }
    //Constraint 5: min, max iL1 and dL1 sizes
    if(iL1size < 2 || iL1size > 64){
        incorrect_flag = 0;
    }
    //Constraint 6: min, max iL1 and dL1 sizes
    if(uL2size < 32 || uL2size > 1024){
        incorrect_flag = 0;
    }
	/*
	// This comes from generateCacheLatencyParams() function
	if(extractConfigPararm(configuration,15)<0 || extractConfigPararm(configuration,16)<0 || extractConfigPararm(configuration,17)<0)
	   incorrect_flag = 0;
	*/
    return incorrect_flag;
}


// Core - FPU - Cache - BP
// My Heuristic
std::string generateNextConfigurationProposal(std::string currentconfiguration,
		std::string bestEXECconfiguration, std::string bestEDPconfiguration,
		int optimizeforEXEC, int optimizeforEDP) {
		
		// just make a temporary variable for the next configuration and keep updating it

		//
        // Some interesting variables in 431project.h include:
        //
        // 1. GLOB_dimensioncardinality
        // 2. GLOB_baseline
        // 3. NUM_DIMS
        // 4. NUM_DIMS_DEPENDENT
        // 5. GLOB_seen_configurations
		
        std::string nextconfiguration = currentconfiguration;

		
		int val = 0;
        // Check if proposed configuration has been see before.
        while (!validateConfiguration(nextconfiguration) || GLOB_seen_configurations[nextconfiguration]){
			
			// I have chosen a threshold of 75 iterations when the program does not leave the while loop
			// I assume this is a safe number of iterations to conclude that the heuristic has explored all possible and valid design points
			// Once the value of 75 is reached, the program breaks
			// It can be concluded that the currentconfiguration cannot be improved anymore as all the dimensions that had to be explored are now explored
			// The program breaks and returns the currentconfiguration

			if(val >= STOPPING_THRESHOLD){
				cout << "val: " << val<<endl;
				isDSEComplete = true;
				return currentconfiguration;
			}

			// update value of val with number of seen configurations
			val += GLOB_seen_configurations[nextconfiguration];

			// index of the dimension being currently explored
			currentlyExploringDim = dimension_vector[dim_index];
			// flag to indicate if this dimension is done running
			currentDimDone = explored_dims_map[dimension_vector[dim_index]];
			count_ = 0;
            // Check if DSE has been completed before and return current
            // configuration.
			
            

		   	
            // placeholder for the configuration
            std::stringstream ss;

            string bestConfig;

            if (optimizeforEXEC == 1)
                bestConfig = bestEXECconfiguration;

            if (optimizeforEDP == 1)
                bestConfig = bestEDPconfiguration;

            // Fill in the dimensions already-scanned with the already-selected best
            // value.
            for (int dim = 0; dim < currentlyExploringDim; ++dim) {
                ss << extractConfigPararm(bestConfig, dim) << " ";
            }
			
            int nextValue = extractConfigPararm(nextconfiguration,
				currentlyExploringDim) + 1;

			if (first_time){

				nextValue = 0;
				first_time = 0;
			}

            if (nextValue >= GLOB_dimensioncardinality[currentlyExploringDim]) {
                nextValue = GLOB_dimensioncardinality[currentlyExploringDim] - 1;
				// update temp val
                currentDimDone = 1;
				// update the dictionary
				explored_dims_map[dimension_vector[dim_index]] = 1;
            }

            ss << nextValue << " ";

			// Fill in remaining independent params with the bestconfig parameters.
			for (int dim = (currentlyExploringDim + 1);
				dim < (NUM_DIMS - NUM_DIMS_DEPENDENT); ++dim) {
				ss << extractConfigPararm(bestConfig, dim) << " ";
			}

			//
			// Last NUM_DIMS_DEPENDENT3 configuration parameters are not independent.
			// They depend on one or more parameters already set. Determine the
			// remaining parameters based on already decided independent ones.
			//
			string configSoFar = ss.str();

			// Populate this object using corresponding parameters from config.
			ss << generateCacheLatencyParams(configSoFar);

			// Configuration is ready now.
			nextconfiguration = ss.str();
			//cout << "here" << nextconfiguration;
			// Make sure we start exploring next dimension in next iteration.
			if (currentDimDone) {
				
				dim_index ++;
				first_time = 1;
				currentDimDone = 0;
				//currentDimDone = false;
			}

			// Signal that DSE is complete after this configuration.
			for(int i = 0; i < 14; i++){
				if(explored_dims_map[dimension_vector[i]] == 1){
					count_ = count_ + 1;
					
				}
				
				//cout << "dimension_vector = " << i << "," << dimension_vector[i] << endl;
				//cout << "dimesnion_map "<< explored_dims_map[dimension_vector[i]] <<endl;
			}
			//cout << "count_ = " <<count_ <<endl;

			
			// if we have explored all 14 dimensions...
			if(count_ == 14){
				count_ = 0;
				dim_index = 0;
				// make all dimensions unexplored
				for (int i = 0; i < 14; i++) {
        			explored_dims_map[dimension_vector[i]] = 0;
    			}
			}

			// Keep the following code in this function as-is.
			if (!validateConfiguration(nextconfiguration)) {
				cerr << "Exiting with error; Configuration Proposal invalid: "
					<< nextconfiguration << endl;
				//continue;
		}
	}
	return nextconfiguration;
	
}



/*
 * Given the current best known configuration, the current configuration,
 * and the globally visible map of all previously investigated configurations,
 * suggest a previously unexplored design point. You will only be allowed to
 * investigate 1000 design points in a particular run, so choose wisely.
 *
 * In the current implementation, we start from the leftmost dimension and
 * explore all possible options for this dimension and then go to the next
 * dimension until the rightmost dimension.
 */

/*
std::string generateNextConfigurationProposal(std::string currentconfiguration,
		std::string bestEXECconfiguration, std::string bestEDPconfiguration,
		int optimizeforEXEC, int optimizeforEDP) {

		
	//
	// Some interesting variables in 431project.h include:
	//
	// 1. GLOB_dimensioncardinality
	// 2. GLOB_baseline
	// 3. NUM_DIMS
	// 4. NUM_DIMS_DEPENDENT
	// 5. GLOB_seen_configurations

	std::string nextconfiguration = currentconfiguration;
	// Check if proposed configuration has been seen before.
	while (!validateConfiguration(nextconfiguration) || GLOB_seen_configurations[nextconfiguration]) {

		// Check if DSE has been completed before and return current
		// configuration.
		if(isDSEComplete) {
			return currentconfiguration;
		}

		std::stringstream ss;

		string bestConfig;
		if (optimizeforEXEC == 1)
			bestConfig = bestEXECconfiguration;

		if (optimizeforEDP == 1)
			bestConfig = bestEDPconfiguration;

		// Fill in the dimensions already-scanned with the already-selected best
		// value.
		for (int dim = 0; dim < currentlyExploringDim; ++dim) {
			ss << extractConfigPararm(bestConfig, dim) << " ";
		}

		// Handling for currently exploring dimension. This is a very dumb
		// implementation.
		int nextValue = extractConfigPararm(nextconfiguration,
				currentlyExploringDim) + 1;

		if (nextValue >= GLOB_dimensioncardinality[currentlyExploringDim]) {
			nextValue = GLOB_dimensioncardinality[currentlyExploringDim] - 1;
			currentDimDone = true;
		}

		ss << nextValue << " ";

		// Fill in remaining independent params with 0.
		for (int dim = (currentlyExploringDim + 1);
				dim < (NUM_DIMS - NUM_DIMS_DEPENDENT); ++dim) {
			ss << "0 ";
		}

		//
		// Last NUM_DIMS_DEPENDENT3 configuration parameters are not independent.
		// They depend on one or more parameters already set. Determine the
		// remaining parameters based on already decided independent ones.
		//
		string configSoFar = ss.str();

		// Populate this object using corresponding parameters from config.
		ss << generateCacheLatencyParams(configSoFar);

		// Configuration is ready now.
		nextconfiguration = ss.str();

		// Make sure we start exploring next dimension in next iteration.
		if (currentDimDone) {
			currentlyExploringDim++;
			currentDimDone = false;
		}

		// Signal that DSE is complete after this configuration.
		if (currentlyExploringDim == (NUM_DIMS - NUM_DIMS_DEPENDENT))
			isDSEComplete = true;

		// Keep the following code in this function as-is.
		if (!validateConfiguration(nextconfiguration)) {
			cerr << "Exiting with error; Configuration Proposal invalid: "
					<< nextconfiguration << endl;
			continue;
		}
	}
	return nextconfiguration;
}

*/










		
