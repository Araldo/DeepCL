// Copyright Hugh Perkins 2014 hughperkins at gmail
//
// This Source Code Form is subject to the terms of the Mozilla Public License, 
// v. 2.0. If a copy of the MPL was not distributed with this file, You can 
// obtain one at http://mozilla.org/MPL/2.0/.

#include <iostream>
#include <cstring>

#include "OpenCLHelper.h"

#include "StatefulTimer.h"
#include "stringhelper.h"

#include "PoolingPropagateGpuNaive.h"

using namespace std;

#undef VIRTUAL
#define VIRTUAL 
#undef STATIC
#define STATIC

VIRTUAL PoolingPropagateGpuNaive::~PoolingPropagateGpuNaive() {
    delete kernel;
}
VIRTUAL void PoolingPropagateGpuNaive::propagate( int batchSize, CLWrapper *inputWrapper, CLWrapper *selectorsWrapper, CLWrapper *outputWrapper ) {
    StatefulTimer::instance()->timeCheck("PoolingPropagateGpuNaive::propagate start" );

    kernel->input( batchSize )->input( inputWrapper )->output( selectorsWrapper )->output( outputWrapper );
    int globalSize = batchSize * numPlanes * outputBoardSize * outputBoardSize;
    int workgroupsize = cl->getMaxWorkgroupSize();
    globalSize = ( ( globalSize + workgroupsize - 1 ) / workgroupsize ) * workgroupsize;
    kernel->run_1d(globalSize, workgroupsize);
    cl->finish();

    StatefulTimer::instance()->timeCheck("PoolingPropagateGpuNaive::propagate end" );
}
PoolingPropagateGpuNaive::PoolingPropagateGpuNaive( OpenCLHelper *cl, bool padZeros, int numPlanes, int inputBoardSize, int poolingSize ) :
        PoolingPropagate( cl, padZeros, numPlanes, inputBoardSize, poolingSize ) {
    string options = "";
    options += " -DgOutputBoardSize=" + toString( outputBoardSize );
    options += " -DgOutputBoardSizeSquared=" + toString( outputBoardSize * outputBoardSize );
    options += " -DgInputBoardSize=" + toString( inputBoardSize );
    options += " -DgInputBoardSizeSquared=" + toString( inputBoardSize * inputBoardSize );
    options += " -DgPoolingSize=" + toString( poolingSize );
    options += " -DgNumPlanes=" + toString( numPlanes );

    // [[[cog
    // import stringify
    // stringify.write_kernel2( "kernel", "cl/pooling.cl", "propagateNaive", 'options' )
    // ]]]
    // generated using cog:
    const char * kernelSource =  
    "// Copyright Hugh Perkins 2014 hughperkins at gmail\n" 
    "//\n" 
    "// This Source Code Form is subject to the terms of the Mozilla Public License,\n" 
    "// v. 2.0. If a copy of the MPL was not distributed with this file, You can\n" 
    "// obtain one at http://mozilla.org/MPL/2.0/.\n" 
    "\n" 
    "// every plane is independent\n" 
    "// every example is independent\n" 
    "// so, globalid can be: [n][plane][outputRow][outputCol]\n" 
    "kernel void propagateNaive( const int batchSize, global const float *input, global int *selectors, global float *output ) {\n" 
    "    const int globalId = get_global_id(0);\n" 
    "\n" 
    "    const int intraBoardOffset = globalId % gOutputBoardSizeSquared;\n" 
    "    const int outputRow = intraBoardOffset / gOutputBoardSize;\n" 
    "    const int outputCol = intraBoardOffset % gOutputBoardSize;\n" 
    "\n" 
    "    const int board2dIdx = globalId / gOutputBoardSizeSquared;\n" 
    "    const int plane = board2dIdx % gNumPlanes;\n" 
    "    const int n = board2dIdx / gNumPlanes;\n" 
    "\n" 
    "    if( n >= batchSize ) {\n" 
    "        return;\n" 
    "    }\n" 
    "\n" 
    "    const int inputRow = outputRow * gPoolingSize;\n" 
    "    const int inputCol = outputCol * gPoolingSize;\n" 
    "    const int inputBoardOffset = ( n * gNumPlanes + plane ) * gInputBoardSizeSquared;\n" 
    "    int selector = 0;\n" 
    "    int poolInputOffset = inputBoardOffset + inputRow * gInputBoardSize + inputCol;\n" 
    "    float maxValue = input[ poolInputOffset ];\n" 
    "    for( int dRow = 0; dRow < gPoolingSize; dRow++ ) {\n" 
    "        for( int dCol = 0; dCol < gPoolingSize; dCol++ ) {\n" 
    "            bool process = ( inputRow + dRow < gInputBoardSize ) && ( inputCol + dCol < gInputBoardSize );\n" 
    "            if( process ) {\n" 
    "                float thisValue = input[ poolInputOffset + dRow * gInputBoardSize + dCol ];\n" 
    "                if( thisValue > maxValue ) {\n" 
    "                    maxValue = thisValue;\n" 
    "                    selector = dRow * gPoolingSize + dCol;\n" 
    "                }\n" 
    "            }\n" 
    "        }\n" 
    "    }\n" 
    "    output[ globalId ] = maxValue;\n" 
    "    selectors[ globalId ] = selector;\n" 
    "}\n" 
    "\n" 
    "";
    kernel = cl->buildKernelFromString( kernelSource, "propagateNaive", options, "cl/pooling.cl" );
    // [[[end]]]
//    kernel = cl->buildKernel( "pooling.cl", "propagateNaive", options );
}

