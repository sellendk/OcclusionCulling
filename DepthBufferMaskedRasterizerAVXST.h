/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
/////////////////////////////////////////////////////////////////////////////////////////////
#ifndef DEPTHBUFFERMASKEDRASTERIZERAVXST_H
#define DEPTHBUFFERMASKEDRASTERIZERAVXST_H

#include "DepthBufferRasterizerAVX.h"

class MaskedOcclusionCulling;

class DepthBufferMaskedRasterizerAVXST : public DepthBufferRasterizerAVX
{
	public:
		DepthBufferMaskedRasterizerAVXST(MaskedOcclusionCulling *moc);
		~DepthBufferMaskedRasterizerAVXST();

		void TransformModelsAndRasterizeToDepthBuffer(CPUTCamera *pCamera, UINT idx);
		void ComputeR2DBTime(UINT idx);

	private:
		void ActiveModels(UINT idx);
		void TransformAndRasterizeMeshes(UINT idx);

		MaskedOcclusionCulling *mMaskedOcclusionCulling;
};

#endif  //DEPTHBUFFERMASKEDRASTERIZERAVXST_H