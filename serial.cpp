#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "common.h"
#include "bin.h"

#define DEBUG 0

//
//  benchmarking program
//
int main( int argc, char **argv )
{    
    int navg,nabsavg=0;
    double davg,dmin, absmin=1.0, absavg=0.0;

    if( find_option( argc, argv, "-h" ) >= 0 )
    {
        printf( "Options:\n" );
        printf( "-h to see this help\n" );
        printf( "-n <int> to set the number of particles\n" );
        printf( "-o <filename> to specify the output file name\n" );
        printf( "-s <filename> to specify a summary file name\n" );
        printf( "-no turns off all correctness checks and particle output\n");
        return 0;
    }
    
    int n = read_int( argc, argv, "-n", 1000 );

    char *savename = read_string( argc, argv, "-o", NULL );
    char *sumname = read_string( argc, argv, "-s", NULL );
    
    FILE *fsave = savename ? fopen( savename, "w" ) : NULL;
    FILE *fsum = sumname ? fopen ( sumname, "a" ) : NULL;

    particle_t *particles = (particle_t*) malloc( n * sizeof(particle_t) );
    double grid_size = set_size( n );
    init_particles( n, particles );

    // Set up bin sizes
    int bin_i, bin_j, num_bins = n % 4 == 0 ? n/4:n/4+1;
    bin_t *bin_list = (bin_t*) malloc(num_bins * sizeof(bin_t));
    if (DEBUG) printf("Testing initializing bins: \n");
    set_grid_size(bin_i, bin_j, num_bins);
    if (DEBUG) printf("There are %d bins, %d per row with %d rows.\n", num_bins, bin_i, bin_j);
    double bin_x = grid_size / bin_i, bin_y = grid_size / bin_j;
    if (DEBUG) printf("The bins are of size %f by %f, err = %f\n", bin_y, bin_x, bin_x*bin_y*num_bins - grid_size*grid_size);
    init_grid(num_bins, bin_list);
    bin_particles(n, particles, num_bins, bin_list, bin_x, bin_y, bin_j);
    

    //
    //  simulate a number of time steps
    //
    double simulation_time = read_timer( );
	
    for( int step = 0; step < NSTEPS; step++ )
    {
	   navg = 0;
       davg = 0.0;
	   dmin = 1.0;
        //
        //  compute forces, this is where the bins come in
        //

        for(int i = 0; i < n; i++)
        {
            particles[i].ax = particles[i].ay = 0;
            int bin_r = particles[i].y / bin_y, bin_c = particles[i].x / bin_x;
            // Traversing the neighbors
            for(int r = max(bin_r - 1, 0); r <= min(bin_r+1, bin_j - 1); r ++)
            {
                for(int c = max(bin_c - 1, 0); c <= min(bin_c+1, bin_i - 1); c++)
                {
                    bin_t neighbor = bin_list[r + c*bin_j];
                    //printf("Neighbor index = %d with size: %d\n", r+c*bin_j, neighbor.bin_size);
                    for(int j = 0; j < neighbor.bin_size; j ++)
                        apply_force(particles[i], particles[neighbor.indeces[j]], &dmin, &davg, &navg);    
                }
            }
        }
 
        //
        //  move particles
        //
        for( int i = 0; i < n; i++ ) 
        {   
            int r_old = particles[i].y / bin_y, c_old = particles[i].x / bin_x;
            move( particles[i] );
            int r = particles[i].y / bin_y, c = particles[i].x / bin_x;
            if (r != r_old || c != c_old)
            {
                remove_particle(bin_list, i, r_old + c_old*bin_j);
                add_particle(bin_list, i, r + c*bin_j);
            }
        }	

        bin_particles(n, particles, num_bins, bin_list, bin_x, bin_y, bin_j);
        
    
        if( find_option( argc, argv, "-no" ) == -1 )
        {
          //
          // Computing statistical data
          //
          if (navg) {
            absavg +=  davg/navg;
            nabsavg++;
          }
          if (dmin < absmin) absmin = dmin;
		
          //
          //  save if necessary
          //
          if( fsave && (step%SAVEFREQ) == 0 )
              save( fsave, n, particles );
        }
    }
    simulation_time = read_timer( ) - simulation_time;
    
    printf( "n = %d, simulation time = %g seconds", n, simulation_time);

    if( find_option( argc, argv, "-no" ) == -1 )
    {
      if (nabsavg) absavg /= nabsavg;
    // 
    //  -The minimum distance absmin between 2 particles during the run of the simulation
    //  -A Correct simulation will have particles stay at greater than 0.4 (of cutoff) with typical values between .7-.8
    //  -A simulation where particles don't interact correctly will be less than 0.4 (of cutoff) with typical values between .01-.05
    //
    //  -The average distance absavg is ~.95 when most particles are interacting correctly and ~.66 when no particles are interacting
    //
    printf( ", absmin = %lf, absavg = %lf", absmin, absavg);
    if (absmin < 0.4) printf ("\nThe minimum distance is below 0.4 meaning that some particle is not interacting");
    if (absavg < 0.8) printf ("\nThe average distance is below 0.8 meaning that most particles are not interacting");
    }
    printf("\n");     

    //
    // Printing summary data
    //
    if( fsum) 
        fprintf(fsum,"%d %g\n",n,simulation_time);
 
    //
    // Clearing space
    //
    if( fsum )
        fclose( fsum );    
    free( particles );
    clear_grid(num_bins, bin_list);
    free(bin_list);
    if( fsave )
        fclose( fsave );
    
    return 0;
}
