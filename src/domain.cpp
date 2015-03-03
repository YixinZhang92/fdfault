#include <iostream>
#include <cassert>
#include <string>
#include "block.hpp"
#include "cartesian.hpp"
#include "domain.hpp"
#include "fd.hpp"
#include "fields.hpp"
//#include "friction.hpp"
//#include "interface.hpp"
#include "rk.hpp"
#include <mpi.h>

using namespace std;

domain::domain(const int ndim_in, const int mode_in, const int nx[3], const int nblocks_in[3], int** nx_block, int** xm_block, double**** x_block, double**** l_block, const int sbporder) {
    // constructor, no default as need to allocate memory
    
	assert(ndim_in == 2 || ndim_in == 3);
	for (int i=0; i<3; i++) {
		assert(nx[i] > 0);
		assert(nblocks_in[i] > 0);
	}
	assert(sbporder >= 2 && sbporder <= 4);
	
	ndim = ndim_in;
    mode = mode_in;
	
	for (int i=0; i<3; i++) {
		nblocks[i] = nblocks_in[i];
	}
	
	nblockstot = nblocks[0]*nblocks[1]*nblocks[2];
	
    // allocate memory for fd coefficients
    
    fd = new fd_type(sbporder);
    
	// set up cartesian type to hold domain decomposition information
	
	cart = new cartesian(ndim, nx, nblocks, nx_block, xm_block, sbporder);
    
    f = new fields(ndim,mode,"elastic",*cart);
	
    // allocate memory and create blocks
    
    allocate_blocks(nx_block, xm_block, x_block, l_block);

    // exchange neighbors to fill in ghost cells
    
    f->exchange_neighbors();
    
    f->write_fields();

}

domain::~domain() {
    // destructor, no default as need to deallocate memory
    
    deallocate_blocks();
    
    delete fd;
	
	delete cart;
    
    delete f;

}

int domain::get_nblocks(const int index) const {
    // returns number of blocks
	assert(index >= 0 && index < 3);
	
    return nblocks[index];
}

int domain::get_nblockstot() const {
	
	return nblockstot;
}

void domain::do_rk_stage(const double dt, const int stage, rk_type& rk) {
    // advances domain fields for one RK stage of one time step
    
    // scale df by RK coefficient
    
    f->scale_df(rk.get_A(stage));
    
    // calculate df
    
    for (int i=0; i<nblocks[0]; i++) {
        for (int j=0; j<nblocks[1]; j++) {
            for (int k=0; k<nblocks[2]; k++) {
                blocks[i][j][k]->calc_df(dt,*f,*fd);
                blocks[i][j][k]->set_boundaries(dt,*f);
            }
        }
    }
    
    // update fields
    
    f->update(rk.get_B(stage));
    
    // exchange neighbors
    
    f->exchange_neighbors();

}

void domain::write_fields() {
    f->write_fields();
}

void domain::allocate_blocks(int** nx_block, int** xm_block, double**** x_block, double**** l_block) {
    // allocate memory for blocks and initialize

    int nxtmp[3];
    int xmtmp[3];
    
    blocks = new block*** [nblocks[0]];
    
    for (int i=0; i<nblocks[0]; i++) {
        blocks[i] = new block** [nblocks[1]];
    }
    
    for (int i=0; i<nblocks[0]; i++) {
        for (int j=0; j<nblocks[1]; j++) {
            blocks[i][j] = new block* [nblocks[2]];
        }
    }
    
    for (int i=0; i<nblocks[0]; i++) {
        for (int j=0; j<nblocks[1]; j++) {
            for (int k=0; k<nblocks[2]; k++) {
                nxtmp[0] = nx_block[0][i];
                nxtmp[1] = nx_block[1][j];
                nxtmp[2] = nx_block[2][k];
                xmtmp[0] = xm_block[0][i];
                xmtmp[1] = xm_block[1][j];
                xmtmp[2] = xm_block[2][k];
                blocks[i][j][k] = new block(ndim, mode, nxtmp, xmtmp, x_block[i][j][k], l_block[i][j][k], *cart, *f, *fd);
            }
        }
    }

}

void domain::deallocate_blocks() {
    // deallocate memory for blocks

    for (int i=0; i<nblocks[0]; i++) {
        for (int j=0; j<nblocks[1]; j++) {
            for (int k=0; k<nblocks[2]; k++) {
                delete blocks[i][j][k];
            }
        }
    }
    
    for (int i=0; i<nblocks[0]; i++) {
        for (int j=0; j<nblocks[1]; j++) {
            delete[] blocks[i][j];
        }
    }
    
    for (int i=0; i<nblocks[0]; i++) {
        delete[] blocks[i];
    }
    
    delete[] blocks;

}