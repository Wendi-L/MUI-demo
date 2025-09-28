/*****************************************************************************
* Multiscale Universal Interface Code Coupling Library Demo 10-2-8           *
*                                                                            *
* Copyright (C) 2023 W. Liu                                                  *
*                                                                            *
* This software is jointly licensed under the Apache License, Version 2.0    *
* and the GNU General Public License version 3, you may use it according     *
* to either.                                                                 *
*                                                                            *
* ** Apache License, version 2.0 **                                          *
*                                                                            *
* Licensed under the Apache License, Version 2.0 (the "License");            *
* you may not use this file except in compliance with the License.           *
* You may obtain a copy of the License at                                    *
*                                                                            *
* http://www.apache.org/licenses/LICENSE-2.0                                 *
*                                                                            *
* Unless required by applicable law or agreed to in writing, software        *
* distributed under the License is distributed on an "AS IS" BASIS,          *
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
* See the License for the specific language governing permissions and        *
* limitations under the License.                                             *
*                                                                            *
* ** GNU General Public License, version 3 **                                *
*                                                                            *
* This program is free software: you can redistribute it and/or modify       *
* it under the terms of the GNU General Public License as published by       *
* the Free Software Foundation, either version 3 of the License, or          *
* (at your option) any later version.                                        *
*                                                                            *
* This program is distributed in the hope that it will be useful,            *
* but WITHOUT ANY WARRANTY; without even the implied warranty of             *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
* GNU General Public License for more details.                               *
*                                                                            *
* You should have received a copy of the GNU General Public License          *
* along with this program.  If not, see <http://www.gnu.org/licenses/>.      *
******************************************************************************/

/**
 * @file heat-left.cpp
 * @author W. Liu
 * @date 16 July 2019
 * @brief Left domain on Aitken's dynamic points to demonstrate MUI coupling algorithm.
 */

#include "mui.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <string>
#include "demo8_config.h"

/*                             Left Domain                       Right Domain             
 * Coarse : +-------+-------+-------+-------o=======+=======o-------+-------+-------+-------+
 *          0       1       2       3       4       5       6       7       8       9      10
 * +: grid points
 * o: interface points
 * -: single domain zone
 * =: overlapping zone
 */

int main( int argc, char ** argv ) {
    using namespace mui;

    const static int N = 70;
    double u1[N], u2[N];

    /// Initialise values from file
    std::string inoutFilenameL = "Resources/left_AITKEN.csv";
    std::fstream inFile;
    std::vector<std::vector<std::string>> content;
    std::vector<std::string> row;
    std::string line, word;
    inFile.open(inoutFilenameL, std::ios::in);
    if(inFile.is_open())
    {
        while(getline(inFile, line)) {
            row.clear();
            std::stringstream str(line);
            while (std::getline(str, word, ',')) {
                row.push_back(word);
           }
           content.push_back(row);
        }

        for ( int i = 0; i <  70; i+=10 ) u1[i] = stod(content[i*0.1+1][1]);

    } else {
        std::cerr<<"left_AITKEN.csv missing" << std::endl;
        u1[0] = 1.;
        for ( int i = 10; i <  70; i+=10 ) u1[i] = 0.;
    }

    inFile.close();

    uniface<demo8_config> interface( "mpi://left/ifs" );

    MPI_Comm  world = mui::mpi_split_by_app();
    MPI_Comm*  Cppworld = &world;

    int rankLocal, sizeLocal;
    MPI_Comm_rank(MPI_COMM_WORLD, &rankLocal);
    MPI_Comm_size(MPI_COMM_WORLD, &sizeLocal);

    int rank, size;
    MPI_Comm_rank( world, &rank );
    MPI_Comm_size( world, &size );

    /// Create folder
    std::string makedirMString = "results_left" + std::to_string(rank);
    mkdir(makedirMString.c_str(), 0777);
    std::string fileAddress(makedirMString);
    std::string makedirMIterString = "results_iteration_left" + std::to_string(rank);
    mkdir(makedirMIterString.c_str(), 0777);
    std::string fileAddressIter(makedirMIterString);

    double        k = 0.515, H = 1;
    double *      u = u1, *v = u2;

    std::vector<std::pair<mui::point1d, double>> ptsVluInit;

    for ( int i = 10; i <  70; i+=10) {
        mui::point1d pt(i);
        ptsVluInit.push_back(std::make_pair(pt,u1[i]));
    }

    // fetch data from the other solver
    sampler_pseudo_nearest_neighbor<demo8_config> s1(30);
    temporal_sampler_exact<demo8_config>  s2;
    algo_aitken<demo8_config> aitken(0.01,1.0);

     // Print off a hello world message
    printf("Hello world from Left rank %d out of %d MUI processors\n",
           rank, size);
           
     // Print off a hello world message
    printf("Hello world from Left rank %d out of %d local processors\n",
           rankLocal, sizeLocal);

    /// Output
    std::ofstream outputFileLeft;
    std::string filenameL = "results_left" + std::to_string(rank) + "/solution-left_AITKEN_0.csv";
    outputFileLeft.open(filenameL);
    outputFileLeft << "\"X\",\"u\"\n";
    for ( int i = 0; i <  70; i+=10 ) outputFileLeft << i * H << "," << u[i] << ", \n";
    outputFileLeft.close();
    std::ofstream outputFileIterLeft;
    std::string filenameIterL = "results_iteration_left" + std::to_string(rank) + "/solution-left_AITKEN_0.csv";
    outputFileIterLeft.open(filenameIterL);
    outputFileIterLeft << "\"X\",\"u\"\n";
    for ( int i = 0; i <  70; i+=10 ) outputFileIterLeft << i * H << "," << u[i] << ", \n";
    outputFileIterLeft.close();

    for ( int t = 1; t <= 10; ++t ) {
		for ( int iter = 1; iter <= 100; ++iter ) {
			printf( "Left grid time %d iteration %d\n", t, iter );

				// push data to the other solver
				interface.push( "u", 40, u[40]);
				interface.commit( t, iter );

				u[60] = interface.fetch( "u0", 60 * H, t, iter, s1, s2, aitken );

				if ((t>=3) && (t<5)) {
					u[58] = interface.fetch( "u0", 58 * H, t, iter, s1, s2, aitken );
				}

				printf( "Left under relaxation factor at t= %d iter= %d is %f\n", t, iter, aitken.get_under_relaxation_factor(t, iter));
				printf( "Left residual L2 Norm at t= %d iter= %d is %f\n", t, iter, aitken.get_residual_L2_Norm(t, iter));

				// calculate 'interior' points
				for ( int i = 10; i <  60; i+=10 ) v[i] = u[i] + k / ( H * H ) * ( u[i - 10] + u[i + 10] - 2 * u[i] );
				// calculate 'boundary' points
				v[0]     = 1.0 + std::sin(2*3.14*0.7962*t);

				v[N - 10] = u[N - 10];

				if ((t>=3) && (t<5)) {
					v[58] = u[58];
				}

			// I/O
			std::swap( u, v );
			/// Output
			std::ofstream outputFileLeft;
			std::string filenameL = "results_iteration_left" + std::to_string(rank) + "/solution-left_AITKEN_"
			  + std::to_string((t-1)*100 + iter) + ".csv";
			outputFileLeft.open(filenameL);
			outputFileLeft << "\"X\",\"u\"\n";
			for ( int i = 0; i <  60; i+=10 ) outputFileLeft << i * H << "," << u[i] << ", \n";

			if ((t>=3) && (t<5)) {
				outputFileLeft << 58 * H << "," << u[58] << ", \n";
			}

			outputFileLeft << 60 * H << "," << u[60] << ", \n";

			outputFileLeft.close();

		}
		/// Output
		std::ofstream outputFileLeft;
		std::string filenameL = "results_left" + std::to_string(rank) + "/solution-left_AITKEN_"
		  + std::to_string(t) + ".csv";
		outputFileLeft.open(filenameL);
		outputFileLeft << "\"X\",\"u\"\n";
		for ( int i = 0; i <  60; i+=10 ) outputFileLeft << i * H << "," << u[i] << ", \n";

		if ((t>=3) && (t<5)) {
			outputFileLeft << 58 * H << "," << u[58] << ", \n";
		}

		outputFileLeft << 60 * H << "," << u[60] << ", \n";

		outputFileLeft.close();
    }

    return 0;
}
