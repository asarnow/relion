/***************************************************************************
 *
 * Author: "Sjors H.W. Scheres"
 * MRC Laboratory of Molecular Biology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This complete copyright notice must be included in any revised version of the
 * source code. Additional authorship citations may be added, but existing
 * author citations must be preserved.
 ***************************************************************************/
#include "src/preprocessing_mpi.h"

void PreprocessingMpi::read(int argc, char **argv)
{
    // Define a new MpiNode
    node = new MpiNode(argc, argv);

    // First read in non-parallelisation-dependent variables
    Preprocessing::read(argc, argv, node->rank);

    // Don't put any output to screen for mpi slaves
    verb = (node->isMaster()) ? 1 : 0;

    // Possibly also read parallelisation-dependent variables here

    // Print out MPI info
	printMpiNodesMachineNames(*node);


}


void PreprocessingMpi::runExtractParticles()
{

	// Each node does part of the work
	long int nr_mics = MDmics.numberOfObjects();
	long int my_first_mic, my_last_mic, my_nr_mics;
	divide_equally(nr_mics, node->size, node->rank, my_first_mic, my_last_mic);
	my_nr_mics = my_last_mic - my_first_mic + 1;

	int barstep;
	if (verb > 0)
	{
		std::cout << " Extracting particles from the micrographs ..." << std::endl;
		init_progress_bar(my_nr_mics);
		barstep = XMIPP_MAX(1, my_nr_mics / 60);

	}

	FileName fn_mic, fn_olddir = "";
	long int imic = 0;
	FOR_ALL_OBJECTS_IN_METADATA_TABLE(MDmics)
	{
		if (imic >= my_first_mic && imic <= my_last_mic)
		{
			MDmics.getValue(EMDL_MICROGRAPH_NAME, fn_mic);

			// Check new-style outputdirectory exists and make it if not!
			FileName fn_dir = getOutputFileNameRoot(fn_mic);
			fn_dir = fn_dir.beforeLastOf("/");
			if (fn_dir != fn_olddir)
			{
				// Make a Particles directory
				int res = system(("mkdir -p " + fn_dir).c_str());
				fn_olddir = fn_dir;
			}

			if (verb > 0 && imic % barstep == 0)
				progress_bar(imic);

			extractParticlesFromFieldOfView(fn_mic, imic);
		}
		imic++;
	}

	// Wait until all nodes have finished to make final star file
	MPI_Barrier(MPI_COMM_WORLD);

	if (verb > 0)
		progress_bar(my_nr_mics);

	if (node->isMaster())
		Preprocessing::joinAllStarFiles();

}



void PreprocessingMpi::run()
{

	// Extract and operate on particles in parallel
	if (do_extract)
	{
		runExtractParticles();

	}
	// The following has not been parallelised....
	else if (fn_operate_in != "" && node->isMaster())
		Preprocessing::runOperateOnInputFile();

	if (verb > 0)
		std::cout << " Done!" <<std::endl;

}
