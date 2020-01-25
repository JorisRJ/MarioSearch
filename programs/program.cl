// OpenCL code
// https://en.wikipedia.org/wiki/OpenCL

#include "program.h"

__kernel void myKernel( global uint* data, global uint* ogpic, global uint* testpic )
{
	int threadid = get_global_id( 0 );
	uint test = testpic[threadid];
	uint og = ogpic[threadid];

	uint r = ( ( test & 0xFF0000 ) >> 16 ) - ( ( og & 0xFF0000 ) >> 16 );
	uint g = ( ( test & 0x00FF00 ) >> 8 ) - ( ( og & 0x00FF00 ) >> 8 );
	uint b = ( test & 0x0000FF ) - ( og & 0x0000FF );

	uint fit = abs( r ) + abs( g ) + abs( b );
	data[threadid] = fit;
	
}