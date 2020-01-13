// OpenCL code
// https://en.wikipedia.org/wiki/OpenCL

#include "program.h"

__kernel void myKernel( global int* data )
{
	int threadid = get_global_id( 0 );
	data[threadid] = threadid;
	
	// get thread id
	//int x = get_global_id( 0 );
	//int y = get_global_id( 1 );
	//int id = x + SCRWIDTH * y;
	//if (id >= (SCRWIDTH * SCRHEIGHT)) return;
	// do calculations
	//float dx = (float)x / (float)SCRWIDTH - 0.5f;
	//float dy = (float)y / (float)SCRHEIGHT - 0.5f;
	//float sqdist = native_sqrt( dx * dx + dy * dy ); // sqrtf causes ptx error..
	// send result to output surface
	//if (sqdist < 0.1f) write_imagef( outimg, (int2)(x, y), (float4)( 1, 1, 1, 1 ) );
}