////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License.  You may obtain a copy
// of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
// License for the specific language governing permissions and limitations
// under the License.
////////////////////////////////////////////////////////////////////////////////

#include "DepthBufferRasterizerOGLST.h"
#include <future>

DepthBufferRasterizerOGLST::DepthBufferRasterizerOGLST()
	: DepthBufferRasterizerOGL()
{
	int size = SCREENH_IN_TILES * SCREENW_IN_TILES;
	mpBin[0] = new UINT[size * MAX_TRIS_IN_BIN_ST];
	mpBinModel[0] = new USHORT[size * MAX_TRIS_IN_BIN_ST];
	mpBinMesh[0] = new USHORT[size * MAX_TRIS_IN_BIN_ST];
	mpNumTrisInBin[0] = new USHORT[size];

	mpBin[1] = new UINT[size * MAX_TRIS_IN_BIN_ST];
	mpBinModel[1] = new USHORT[size * MAX_TRIS_IN_BIN_ST];
	mpBinMesh[1] = new USHORT[size * MAX_TRIS_IN_BIN_ST];
	mpNumTrisInBin[1] = new USHORT[size];

	Osmesa = std::make_unique<OSMesaPipeline>();
	
}

DepthBufferRasterizerOGLST::~DepthBufferRasterizerOGLST()
{
	SAFE_DELETE_ARRAY(mpBin[0]);
	SAFE_DELETE_ARRAY(mpBinModel[0]);
	SAFE_DELETE_ARRAY(mpBinMesh[0]);
	SAFE_DELETE_ARRAY(mpNumTrisInBin[0]);

	SAFE_DELETE_ARRAY(mpBin[1]);
	SAFE_DELETE_ARRAY(mpBinModel[1]);
	SAFE_DELETE_ARRAY(mpBinMesh[1]);
	SAFE_DELETE_ARRAY(mpNumTrisInBin[1]);
}

//------------------------------------------------------------------------------
// * Determine if the occludee model is inside view frustum
// * Transform the occluder models on the CPU
// * Bin the occluder triangles into tiles that the frame buffer is divided into
// * Rasterize the occluder triangles to the CPU depth buffer
//-------------------------------------------------------------------------------
void DepthBufferRasterizerOGLST::TransformModelsAndRasterizeToDepthBuffer(CPUTCamera *pCamera, UINT idx)
{
	QueryPerformanceCounter(&mStartTime[idx]);
	mpCamera[idx] = pCamera;

	BoxTestSetupScalar setup;
	setup.Init(mpViewMatrix[idx], mpProjMatrix[idx], viewportMatrix, mpCamera[idx], mOccluderSizeThreshold);

	if(mEnableFCulling)
	{
		// gather all models with all their parameters and check for frustum culling on the GPU
		for(UINT i = 0; i < mNumModels1; i++)
		{
			// Mesa link inside InsideViewFrustum
			mpTransformedModels1[i].InsideViewFrustum(setup, idx);
			mpTransformedModels1[i].SetCumulativeMatrix(mpViewMatrix[idx], mpProjMatrix[idx], idx);
		}
	}
	else
	{
		for(UINT i = 0; i < mNumModels1; i++)
		{
			// reimplement for Mesa
			mpTransformedModels1[i].TooSmall(setup, idx);
		}
	}

	// Recalculate mViewProjViewport with the identity matrix instead of viewportMatrix.
	// This is required in order to get the proper transformation in the mesa context.
	setup.mViewProjViewport = mpViewMatrix[idx] * mpProjMatrix[idx];
	//setup.mViewProjViewport = setup.mViewProjViewport * float4x4Identity(); // not required for identity matrix

	ActiveModels(idx);
	TransformMeshes(idx);
	
	// After meshes are transformed, they are rendered to the depth buffer
	float* pDepthBuffer = (float*)mpRenderTargetPixels[idx];
	//auto mesa_exec = std::async(std::launch::async, &(OSMesaPipeline::start), Osmesa.get(), mFinalXformedPos, pDepthBuffer);
	Osmesa->start(mFinalXformedPos, pDepthBuffer);
	mFinalXformedPos.clear();


	QueryPerformanceCounter(&mStopTime[idx][0]);
	mRasterizeTime[mTimeCounter++] = ((double)(mStopTime[idx][0].QuadPart - mStartTime[idx].QuadPart)) / ((double)glFrequency.QuadPart);
	mTimeCounter = mTimeCounter >= AVG_COUNTER ? 0 : mTimeCounter;
}

void DepthBufferRasterizerOGLST::ActiveModels(UINT idx)
{
	ResetActive(idx);
	for (UINT i = 0; i < mNumModels1; i++)
	{
		if(mpTransformedModels1[i].IsRasterized2DB(idx))
		{
			Activate(i, idx);
		}
	}
}


//-------------------------------------------------------------------
// Transforms the occluder vertices to screen space once every frame
//-------------------------------------------------------------------
void DepthBufferRasterizerOGLST::TransformMeshes(UINT idx)
{
	for(UINT active = 0; active < mNumModelsA[idx]; active++)
    {
		UINT ss = mpModelIndexA[idx][active];
		UINT thisSurfaceVertexCount = mpTransformedModels1[ss].GetNumVertices();
        
        mpTransformedModels1[ss].TransformMeshes(0, thisSurfaceVertexCount - 1, mpCamera[idx], idx);
		std::vector<float4> b = mpTransformedModels1[ss].GetAllXformedPos1();
		mFinalXformedPos.insert(mFinalXformedPos.end(), b.begin(), b.end());
    }
}

//-------------------------------------------------
// Bins the transformed triangles into tiles
//-------------------------------------------------
void DepthBufferRasterizerOGLST::BinTransformedMeshes(UINT idx)
{
	// Reset the bin count.  Note the data layout makes this traversal a bit awkward.
    // We can't just use memset() because the last array index isn't what's varying.
    // However, this should make the real use of this structure go faster.
	for(UINT yy = 0; yy < SCREENH_IN_TILES; yy++)
    {
		UINT offset = YOFFSET1_ST * yy;
        for(UINT xx = 0; xx < SCREENW_IN_TILES; xx++)
        {
			UINT index = offset + (XOFFSET1_ST * xx);
            mpNumTrisInBin[idx][index] = 0;
	    }
    }

	// Now, process all of the surfaces that contain this task's triangle range.
	for(UINT active = 0; active < mNumModelsA[idx]; active++)
    {
		UINT ss = mpModelIndexA[idx][active];
		UINT thisSurfaceTriangleCount = mpTransformedModels1[ss].GetNumTriangles();
        
        mpTransformedModels1[ss].BinTransformedTrianglesST(0, ss, 0, thisSurfaceTriangleCount - 1, mpBin[idx], mpBinModel[idx], mpBinMesh[idx], mpNumTrisInBin[idx], idx);
	}
}

//-------------------------------------------------------------------------------
// For each tile go through all the bins and process all the triangles in it.
// Rasterize each triangle to the CPU depth buffer. 
//-------------------------------------------------------------------------------
void DepthBufferRasterizerOGLST::RasterizeBinnedTrianglesToDepthBuffer(UINT tileId, UINT idx)
{
	float* pDepthBuffer = (float*)mpRenderTargetPixels[idx]; 

	// Based on TaskId determine which tile to process
	UINT screenWidthInTiles = SCREENW/TILE_WIDTH_IN_PIXELS;
    UINT tileX = tileId % screenWidthInTiles;
    UINT tileY = tileId / screenWidthInTiles;

    int tileStartX = tileX * TILE_WIDTH_IN_PIXELS;
	int tileEndX   = min(tileStartX + TILE_WIDTH_IN_PIXELS - 1, SCREENW - 1);
	
	int tileStartY = tileY * TILE_HEIGHT_IN_PIXELS;
	int tileEndY   = min(tileStartY + TILE_HEIGHT_IN_PIXELS - 1, SCREENH - 1);

	UINT bin = 0;
	UINT binIndex = 0;
	UINT offset1 = YOFFSET1_ST * tileY + XOFFSET1_ST * tileX;
	UINT offset2 = YOFFSET2_ST * tileY + XOFFSET2_ST * tileX;
	UINT numTrisInBin = mpNumTrisInBin[idx][offset1 + bin];

	ClearDepthTile(tileStartX, tileStartY, tileEndX + 1, tileEndY + 1, idx);

	float4 xformedPos[3];
	bool done = false;
	bool allBinsEmpty = true;
	mNumRasterizedTris[idx][tileId] = numTrisInBin;

	while(!done)
	{
		// Loop through all the bins and process the binned traingles
		while(numTrisInBin <= 0)
		{
			// This bin is empty.  Move to next bin.
			if(++bin >= 1)
			{
				break;
			}
			numTrisInBin = mpNumTrisInBin[idx][offset1 + bin];
			mNumRasterizedTris[idx][tileId] += numTrisInBin;
			binIndex = 0; // Slightly inefficient.  We set it every time through this loop.  Could do only once.
		}
		if(!numTrisInBin)
		{
			break; // No more tris in the bins
		}
		USHORT modelId = mpBinModel[idx][offset2 + bin * MAX_TRIS_IN_BIN_ST + binIndex];
		USHORT meshId = mpBinMesh[idx][offset2 + bin * MAX_TRIS_IN_BIN_ST + binIndex];
		UINT triIdx = mpBin[idx][offset2 + bin * MAX_TRIS_IN_BIN_ST + binIndex];
		mpTransformedModels1[modelId].Gather((float*)xformedPos, meshId, triIdx, idx);
		allBinsEmpty = false;
		
		++binIndex;
		--numTrisInBin;
		
		done = bin >= NUM_XFORMVERTS_TASKS;
		
		if(allBinsEmpty)
		{
			return;
		}

		// use fixed-point only for X and Y.  Avoid work for Z and W.
        int fxPtX[3], fxPtY[3];
		float Z[3];
		for(UINT i = 0; i < 3; i++)
		{
			fxPtX[i] = (int)(xformedPos[i].x + 0.5);
			fxPtY[i] = (int)(xformedPos[i].y + 0.5);
			Z[i] = xformedPos[i].z;
		}

		// Fab(x, y) =     Ax       +       By     +      C              = 0
		// Fab(x, y) = (ya - yb)x   +   (xb - xa)y + (xa * yb - xb * ya) = 0
		// Compute A = (ya - yb) for the 3 line segments that make up each triangle
		int A0 = fxPtY[1] - fxPtY[2];
		int A1 = fxPtY[2] - fxPtY[0];
		int A2 = fxPtY[0] - fxPtY[1];

		// Compute B = (xb - xa) for the 3 line segments that make up each triangle
		int B0 = fxPtX[2] - fxPtX[1];
		int B1 = fxPtX[0] - fxPtX[2];
		int B2 = fxPtX[1] - fxPtX[0];

		// Compute C = (xa * yb - xb * ya) for the 3 line segments that make up each triangle
		int C0 = fxPtX[1] * fxPtY[2] - fxPtX[2] * fxPtY[1];
		int C1 = fxPtX[2] * fxPtY[0] - fxPtX[0] * fxPtY[2];
		int C2 = fxPtX[0] * fxPtY[1] - fxPtX[1] * fxPtY[0];

		// Compute triangle area
		int triArea = (fxPtX[1] - fxPtX[0]) * (fxPtY[2] - fxPtY[0]) - (fxPtX[0] - fxPtX[2]) * (fxPtY[0] - fxPtY[1]);
		float oneOverTriArea = (1.0f/float(triArea));

		Z[1] = (Z[1] - Z[0]) * oneOverTriArea;
		Z[2] = (Z[2] - Z[0]) * oneOverTriArea;

		// Use bounding box traversal strategy to determine which pixels to rasterize 
		int startX = max(min(min(fxPtX[0], fxPtX[1]), fxPtX[2]), tileStartX) & int(0xFFFFFFFE);
		int endX   = min(max(max(fxPtX[0], fxPtX[1]), fxPtX[2]), tileEndX+1);

		int startY = max(min(min(fxPtY[0], fxPtY[1]), fxPtY[2]), tileStartY) & int(0xFFFFFFFE);
		int endY   = min(max(max(fxPtY[0], fxPtY[1]), fxPtY[2]), tileEndY+1);

		int rowIdx = (startY * SCREENW + startX);
		int col = startX;
		int row = startY;
		
		// Incrementally compute Fab(x, y) for all the pixels inside the bounding box formed by (startX, endX) and (startY, endY)
		int alpha0 = (A0 * col) + (B0 * row) + C0;
		int beta0 = (A1 * col) + (B1 * row) + C1;
		int gama0 = (A2 * col) + (B2 * row) + C2;

		float zx = A1 * Z[1] + A2 * Z[2];

		for(int r = startY; r < endY; r++,
									  row++,
									  rowIdx = rowIdx + SCREENW,
									  alpha0 += B0,
									  beta0 += B1,
									  gama0 += B2)									 
		{
			// Compute barycentric coordinates 
			int index = rowIdx;
			int alpha = alpha0;
			int beta = beta0;
			int gama = gama0;

			float depth = Z[0] + Z[1] * beta + Z[2] * gama;
			
			for(int c = startX; c < endX; c++,
   										  index++,
										  alpha += A0,
										  beta  += A1,
										  gama  += A2,
										  depth += zx)
			{
				//Test Pixel inside triangle
				int mask = alpha | beta | gama;
					
				float previousDepthValue = pDepthBuffer[index];
				float mergedDepth = max(depth, previousDepthValue);				
				float finaldepth = mask < 0 ? previousDepthValue : mergedDepth;
				
				pDepthBuffer[index] = finaldepth;
			}//for each column											
		}// for each row
	}// for each triangle*/
}

void DepthBufferRasterizerOGLST::ComputeR2DBTime(UINT idx)
{
}