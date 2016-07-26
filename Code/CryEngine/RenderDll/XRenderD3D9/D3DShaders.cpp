// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   D3DShaders.cpp : Direct3D specific effectors/shaders functions implementation.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include "../Common/DevBuffer.h"
#include <Cry3DEngine/I3DEngine.h>
#include <CrySystem/Profilers/IStatoscope.h>
#include <CrySystem/File/IResourceManager.h>

//==================================================================================

bool CShader::FXSetTechnique(const CCryNameTSCRC& Name)
{
	assert(gRenDev->m_pRT->IsRenderThread());

	uint32 i;
	SShaderTechnique* pTech = NULL;
	for (i = 0; i < m_HWTechniques.Num(); i++)
	{
		pTech = m_HWTechniques[i];
		if (pTech && Name == pTech->m_NameCRC)
			break;
	}

	CRenderer* rd = gRenDev;

	if (i == m_HWTechniques.Num())
	{
		// not found and not set
		rd->m_RP.m_pShader = NULL;
		rd->m_RP.m_nShaderTechnique = -1;
		rd->m_RP.m_pCurTechnique = NULL;
		return false;
	}

	rd->m_RP.m_pShader = this;
	rd->m_RP.m_nShaderTechnique = i;
	rd->m_RP.m_pCurTechnique = m_HWTechniques[i];

	return true;
}

bool CShader::FXSetCSFloat(const CCryNameR& NameParam, const Vec4 fParams[], int nParams)
{
	CRenderer* rd = gRenDev;
	if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique)
		return false;
	SShaderPass* pPass = rd->m_RP.m_pCurPass;
	if (!pPass)
		return false;
	CHWShader_D3D* curCS = (CHWShader_D3D*)pPass->m_CShader;
	if (!curCS)
		return false;
	SCGBind* pBind = curCS->mfGetParameterBind(NameParam);
	if (!pBind)
		return false;
	curCS->mfParameterReg(pBind->m_dwBind, pBind->m_dwCBufSlot, eHWSC_Compute, &fParams[0].x, nParams, curCS->m_pCurInst->m_nMaxVecs[pBind->m_dwCBufSlot]);
	return true;
}

bool CShader::FXSetPSFloat(const CCryNameR& NameParam, const Vec4* fParams, int nParams)
{
	CRenderer* rd = gRenDev;
	if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique)
		return false;
	SShaderPass* pPass = rd->m_RP.m_pCurPass;
	if (!pPass)
		return false;
	CHWShader_D3D* curPS = (CHWShader_D3D*)pPass->m_PShader;
	if (!curPS)
		return false;
	SCGBind* pBind = curPS->mfGetParameterBind(NameParam);
	if (!pBind)
		return false;

	curPS->mfParameterReg(pBind->m_dwBind, pBind->m_dwCBufSlot, eHWSC_Pixel, &fParams[0].x, nParams, curPS->m_pCurInst->m_nMaxVecs[pBind->m_dwCBufSlot]);
	return true;
}

bool CShader::FXSetVSFloat(const CCryNameR& NameParam, const Vec4* fParams, int nParams)
{
	CRenderer* rd = gRenDev;
	if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique)
		return false;
	SShaderPass* pPass = rd->m_RP.m_pCurPass;
	if (!pPass)
		return false;
	CHWShader_D3D* curVS = (CHWShader_D3D*)pPass->m_VShader;
	if (!curVS)
		return false;
	SCGBind* pBind = curVS->mfGetParameterBind(NameParam);
	if (!pBind)
		return false;

	curVS->mfParameterReg(pBind->m_dwBind, pBind->m_dwCBufSlot, eHWSC_Vertex, &fParams[0].x, nParams, curVS->m_pCurInst->m_nMaxVecs[pBind->m_dwCBufSlot]);
	return true;
}

bool CShader::FXSetGSFloat(const CCryNameR& NameParam, const Vec4* fParams, int nParams)
{
	CRenderer* rd = gRenDev;
	if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique)
		return false;
	SShaderPass* pPass = rd->m_RP.m_pCurPass;
	if (!pPass)
		return false;
	CHWShader_D3D* curGS = (CHWShader_D3D*)pPass->m_GShader;
	if (!curGS)
		return false;
	SCGBind* pBind = curGS->mfGetParameterBind(NameParam);
	if (!pBind)
		return false;
	curGS->mfParameterReg(pBind->m_dwBind, pBind->m_dwCBufSlot, eHWSC_Geometry, &fParams[0].x, nParams, curGS->m_pCurInst->m_nMaxVecs[pBind->m_dwCBufSlot]);
	return true;
}

bool CShader::FXBegin(uint32* uiPassCount, uint32 nFlags)
{
	CRenderer* rd = gRenDev;
	if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique || !rd->m_RP.m_pCurTechnique->m_Passes.Num())
		return false;
	*uiPassCount = rd->m_RP.m_pCurTechnique->m_Passes.Num();
	rd->m_RP.m_nFlagsShaderBegin = nFlags;
	rd->m_RP.m_pCurPass = &rd->m_RP.m_pCurTechnique->m_Passes[0];
	return true;
}

bool CShader::FXBeginPass(uint32 uiPass)
{
	FUNCTION_PROFILER_RENDER_FLAT
	CD3D9Renderer* rd = gcpRendD3D;
	if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique || uiPass >= rd->m_RP.m_pCurTechnique->m_Passes.Num())
		return false;
	rd->m_RP.m_pCurPass = &rd->m_RP.m_pCurTechnique->m_Passes[uiPass];
	SShaderPass* pPass = rd->m_RP.m_pCurPass;
	//assert (pPass->m_VShader && pPass->m_PShader);
	//if (!pPass->m_VShader || !pPass->m_PShader)
	//  return false;
	CHWShader_D3D* curVS = (CHWShader_D3D*)pPass->m_VShader;
	CHWShader_D3D* curPS = (CHWShader_D3D*)pPass->m_PShader;
	CHWShader_D3D* curGS = (CHWShader_D3D*)pPass->m_GShader;
	CHWShader_D3D* curCS = (CHWShader_D3D*)pPass->m_CShader;

	bool bResult = true;

	// Set Pixel-shader and all associated textures
	if (curPS)
	{
		if (rd->m_RP.m_nFlagsShaderBegin & FEF_DONTSETTEXTURES)
			bResult &= curPS->mfSet(0);
		else
			bResult &= curPS->mfSet(HWSF_SETTEXTURES);
		curPS->mfSetParametersPI(NULL, NULL);
	}
	// Set Vertex-shader
	if (curVS)
	{
		curVS->mfSet(0);
		curVS->mfSetParametersPI(NULL, rd->m_RP.m_pShader);
	}

	// Set Geometry-shader
	if (curGS)
		bResult &= curGS->mfSet(0);
	else
		CHWShader_D3D::mfBindGS(NULL, NULL);

	// Set Compute-shader
	if (curCS)
	{
		if (rd->m_RP.m_nFlagsShaderBegin & FEF_DONTSETTEXTURES)
			bResult &= curCS->mfSet(0);
		else
			bResult &= curCS->mfSet(HWSF_SETTEXTURES);
	}
	else
		CHWShader_D3D::mfBindCS(NULL, NULL);

	if (!(rd->m_RP.m_nFlagsShaderBegin & FEF_DONTSETSTATES))
	{
		rd->FX_SetState(pPass->m_RenderState);
		if (pPass->m_eCull != -1)
			rd->D3DSetCull((ECull)pPass->m_eCull);
	}

	return bResult;
}

bool CShader::FXEndPass()
{
	CRenderer* rd = gRenDev;
	if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique || !rd->m_RP.m_pCurTechnique->m_Passes.Num())
		return false;
	rd->m_RP.m_pCurPass = NULL;
	return true;
}

bool CShader::FXEnd()
{
	CRenderer* rd = gRenDev;
	if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique)
		return false;
	return true;
}

bool CShader::FXCommit(const uint32 nFlags)
{
	gcpRendD3D->FX_Commit();

	return true;
}

void* CHWShader_D3D::GetVSDataForDecl(const D3D11_INPUT_ELEMENT_DESC* pDecl, int nCount, int& nDataSize)
{
	nDataSize = 0;
	CShader* pSh = CShaderMan::s_ShaderFPEmu;
	if (!pSh || !pSh->m_HWTechniques.Num() || !nCount)
		return NULL;
	uint32 i, j;

	SShaderTechnique* pTech = NULL;
	for (i = 0; i < pSh->m_HWTechniques.Num(); i++)
	{
		if (!stricmp(pSh->m_HWTechniques[i]->m_NameStr.c_str(), "InputLayout"))
		{
			pTech = pSh->m_HWTechniques[i];
			break;
		}
	}
	if (!pTech || !pTech->m_Passes.Num())
		return NULL;
	SShaderPass& Pass = pTech->m_Passes[0];
	CHWShader_D3D* pVS = (CHWShader_D3D*)Pass.m_VShader;
	if (!pVS)
		return NULL;

	uint32 nMask = 0;
	//m_RP.m_FlagsShader_LT = m_RP.m_TI[m_RP.m_nProcessThreadID].m_eCurColorOp[0] | (m_RP.m_TI[m_RP.m_nProcessThreadID].m_eCurAlphaOp[0] << 8) | (m_RP.m_TI[m_RP.m_nProcessThreadID].m_eCurColorArg[0] << 16) | (m_RP.m_TI[m_RP.m_nProcessThreadID].m_eCurAlphaArg[0] << 24);

	for (i = 0; i < nCount; i++)
	{
		const D3D11_INPUT_ELEMENT_DESC& Desc = pDecl[i];
		if (Desc.InputSlot != 0)
		{
			nMask |= 1 << Desc.InputSlot;
			if (nMask & VSM_TANGENTS)
			{
				for (j = 0; j < nCount; j++)
				{
					const D3D11_INPUT_ELEMENT_DESC& Desc2 = pDecl[j];
					if (!strcmp(Desc2.SemanticName, "BITANGENT") || !strcmp(Desc2.SemanticName, "BINORMAL"))
					{
						nMask |= (1 << VSF_QTANGENTS);
						break;
					}
				}
			}
		}
		else if (!strcmp(Desc.SemanticName, "POSITION"))
		{
			if (Desc.Format == DXGI_FORMAT_R32G32_FLOAT)
				nMask |= 0 << 8;
			else if (Desc.Format == DXGI_FORMAT_R32G32B32_FLOAT)
				nMask |= 1 << 8;
			else if (Desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT || Desc.Format == DXGI_FORMAT_R32G32B32A32_FLOAT)
				nMask |= 2 << 8;
			else
				assert(0);
		}
		else if (!strcmp(Desc.SemanticName, "TEXCOORD"))
		{
			int nShift = 0;
			if (Desc.SemanticIndex == 0)
			{
				nMask |= eCA_Texture << 16;
				nShift = 10;
				if (Desc.Format == DXGI_FORMAT_R32G32_FLOAT || Desc.Format == DXGI_FORMAT_R16G16_FLOAT)
					nMask |= 0 << nShift;
				else if (Desc.Format == DXGI_FORMAT_R32G32B32_FLOAT)
					nMask |= 1 << nShift;
				else if (Desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT || Desc.Format == DXGI_FORMAT_R32G32B32A32_FLOAT)
					nMask |= 2 << nShift;
				else
					assert(0);
			}
			else if (Desc.SemanticIndex == 1)
			{
				if (nMask & (eCA_Constant << 19))
				{
					// PSIZE and TEX1 used together
					nMask &= ~(0x7 << 19);
					nMask |= eCA_Previous << 19;
				}
				else
					nMask |= eCA_Texture1 << 19;
			}
			else
				assert(0);
		}
		else if (!strcmp(Desc.SemanticName, "COLOR"))
		{
			int nShift = 0;
			if (Desc.SemanticIndex == 0)
			{
				nMask |= eCA_Diffuse << 24;
				nShift = 12;
				if (Desc.Format == DXGI_FORMAT_R32G32B32_FLOAT)
					nMask |= 1 << nShift;
				else if (Desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || Desc.Format == DXGI_FORMAT_R32G32B32A32_FLOAT)
					nMask |= 2 << nShift;
				else
					assert(0);
			}
			else if (Desc.SemanticIndex == 1)
			{
				nMask |= eCA_Specular << 27;
			}
			else
			{
				assert(0);
			}
		}
		else if (!strcmp(Desc.SemanticName, "NORMAL"))
		{
			if (Desc.SemanticIndex == 0)
				nMask |= eCA_Normal << 27;
			else
			{
				assert(0);
			}
			//assert (Desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM);
		}
		else if (!strcmp(Desc.SemanticName, "PSIZE"))
		{
			if (Desc.SemanticIndex == 0)
			{
				if (nMask & (eCA_Texture1 << 19))
				{
					// PSIZE and TEX1 used together
					nMask &= ~(0x7 << 19);
					nMask |= eCA_Previous << 19;
				}
				else
					nMask |= eCA_Constant << 19;
			}
			else
			{
				assert(0);
			}
			assert(Desc.Format == DXGI_FORMAT_R32G32B32A32_FLOAT);
		}
		else
		{
			assert(0);
		}
	}

	SShaderCombIdent Ident;
	Ident.m_LightMask = nMask;
	Ident.m_RTMask = 0;
	Ident.m_MDMask = 0;
	Ident.m_MDVMask = 0;
	Ident.m_GLMask = 0;

	SHWSInstance* pI = pVS->m_pCurInst;

	uint32 nFlags = HWSF_STOREDATA;
	SHWSInstance* pInst = pVS->mfGetInstance(pSh, Ident, nFlags);
	if (!pVS->mfCheckActivation(pSh, pInst, nFlags))
	{
		pVS->m_pCurInst = pI;
		return NULL;
	}
	pVS->m_pCurInst = pI;

	nDataSize = pInst->m_nDataSize;
	return pInst->m_pShaderData;
}

//===================================================================================

void CRenderer::RefreshSystemShaders()
{
	// make sure all system shaders are properly refreshed during loading!
#if defined(FEATURE_SVO_GI)
	gRenDev->m_cEF.mfRefreshSystemShader("Total_Illumination", CShaderMan::s_ShaderSVOGI);
#endif
	gRenDev->m_cEF.mfRefreshSystemShader("Common", CShaderMan::s_ShaderCommon);
	gRenDev->m_cEF.mfRefreshSystemShader("Debug", CShaderMan::s_ShaderDebug);
	gRenDev->m_cEF.mfRefreshSystemShader("DeferredCaustics", CShaderMan::s_ShaderDeferredCaustics);
	gRenDev->m_cEF.mfRefreshSystemShader("DeferredRain", CShaderMan::s_ShaderDeferredRain);
	gRenDev->m_cEF.mfRefreshSystemShader("DeferredSnow", CShaderMan::s_ShaderDeferredSnow);
	gRenDev->m_cEF.mfRefreshSystemShader("DeferredShading", CShaderMan::s_shDeferredShading);
	gRenDev->m_cEF.mfRefreshSystemShader("DepthOfField", CShaderMan::s_shPostDepthOfField);
	gRenDev->m_cEF.mfRefreshSystemShader("DXTCompress", CShaderMan::s_ShaderDXTCompress);
	gRenDev->m_cEF.mfRefreshSystemShader("FarTreeSprites", CShaderMan::s_ShaderTreeSprites);
	gRenDev->m_cEF.mfRefreshSystemShader("LensOptics", CShaderMan::s_ShaderLensOptics);
	gRenDev->m_cEF.mfRefreshSystemShader("SoftOcclusionQuery", CShaderMan::s_ShaderSoftOcclusionQuery);
	gRenDev->m_cEF.mfRefreshSystemShader("MotionBlur", CShaderMan::s_shPostMotionBlur);
	gRenDev->m_cEF.mfRefreshSystemShader("OcclusionTest", CShaderMan::s_ShaderOcclTest);
	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);
	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsRenderModes", CShaderMan::s_shPostEffectsRenderModes);
	gRenDev->m_cEF.mfRefreshSystemShader("ShadowBlur", CShaderMan::s_ShaderShadowBlur);
	gRenDev->m_cEF.mfRefreshSystemShader("Stereo", CShaderMan::s_ShaderStereo);
	gRenDev->m_cEF.mfRefreshSystemShader("Sunshafts", CShaderMan::s_shPostSunShafts);
	gRenDev->m_cEF.mfRefreshSystemShader("LightPropagationVolumes", CShaderMan::s_ShaderLPV);
	gRenDev->m_cEF.mfRefreshSystemShader("Clouds", CShaderMan::s_ShaderClouds);
}

bool CD3D9Renderer::FX_SetFPMode()
{
	assert(gRenDev->m_pRT->IsRenderThread());

	if (!(m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & (RBPF_FP_DIRTY | RBPF_FP_MATRIXDIRTY)) && CShaderMan::s_ShaderFPEmu == m_RP.m_pShader)
		return true;
	if (m_bDeviceLost)
		return false;
	m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags &= ~RBPF_FP_DIRTY | RBPF_FP_MATRIXDIRTY;
	m_RP.m_ObjFlags &= ~FOB_TRANS_MASK;
	m_RP.m_pCurObject = m_RP.m_pIdendityRenderObject;
	CShader* pSh = CShaderMan::s_ShaderFPEmu;
	if (!pSh || !pSh->m_HWTechniques.Num())
		return false;
	m_RP.m_FlagsShader_LT = m_RP.m_TI[m_RP.m_nProcessThreadID].m_eCurColorOp | (m_RP.m_TI[m_RP.m_nProcessThreadID].m_eCurAlphaOp << 8) | (m_RP.m_TI[m_RP.m_nProcessThreadID].m_eCurColorArg << 16) | (m_RP.m_TI[m_RP.m_nProcessThreadID].m_eCurAlphaArg << 24);
	if (CTexture::s_TexStages[0].m_DevTexture && CTexture::s_TexStages[0].m_DevTexture->IsCube())
		m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_CUBEMAP0];
	else
		m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_CUBEMAP0];

	m_RP.m_pShader = pSh;
	m_RP.m_pCurTechnique = pSh->m_HWTechniques[0];

	bool bRes = pSh->FXBegin(&m_RP.m_nNumRendPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	if (!bRes)
		return false;
	bRes = pSh->FXBeginPass(0);
	FX_Commit();
	return bRes;
}

void CShaderMan::mfCheckObjectDependParams(std::vector<SCGParam>& PNoObj, std::vector<SCGParam>& PObj, EHWShaderClass eSH, CShader* pFXShader)
{
	if (!PNoObj.size())
		return;
	uint32 i;
	for (i = 0; i < PNoObj.size(); i++)
	{
		SCGParam* prNoObj = &PNoObj[i];
		if ((prNoObj->m_eCGParamType & 0xff) == ECGP_PM_Tweakable)
		{
			int nType = prNoObj->m_eCGParamType & 0xff;
			prNoObj->m_eCGParamType = (ECGParam)nType;
		}

		if (prNoObj->m_Flags & PF_INSTANCE)
		{
			PObj.push_back(PNoObj[i]);
			PNoObj.erase(PNoObj.begin() + i);
			i--;
		}
		else if (prNoObj->m_Flags & PF_MATERIAL)
		{
			PNoObj.erase(PNoObj.begin() + i);
			i--;
		}
		else if (prNoObj->m_Flags & PF_LIGHT)
		{
			PNoObj.erase(PNoObj.begin() + i);
			i--;
		}
		else if (prNoObj->m_Flags & (PF_GLOBAL | PF_SHADOWGEN))
		{
			bool bCam = (prNoObj->m_eCGParamType == ECGP_Matr_PF_ViewProjMatrix || prNoObj->m_eCGParamType == ECGP_Matr_PF_ViewProjZeroMatrix || prNoObj->m_eCGParamType == ECGP_PF_NearFarDist);
			CHWShader_D3D::mfAddGlobalParameter(PNoObj[i], eSH, (prNoObj->m_Flags & PF_SHADOWGEN) != 0, bCam);
			PNoObj.erase(PNoObj.begin() + i);
			i--;
		}
	}
	if (PObj.size())
	{
		int n = -1;
		for (i = 0; i < PObj.size(); i++)
		{
			if (PObj[i].m_eCGParamType == ECGP_Matr_PI_ViewProj)
			{
				n = i;
				break;
			}
		}
		if (n > 0)
		{
			SCGParam pr = PObj[n];
			PObj[n] = PObj[0];
			PObj[0] = pr;
		}
	}
}

void CHWShader_D3D::mfLogShaderCacheMiss(SHWSInstance* pInst)
{
	CShaderMan& Man = gRenDev->m_cEF;

	// update the stats
	Man.m_ShaderCacheStats.m_nGlobalShaderCacheMisses++;

	// don't do anything else if CVar is disabled and no callback is registered
	if (CRenderer::CV_r_shaderslogcachemisses == 0 && Man.m_ShaderCacheMissCallback == 0)
		return;

	char nameCache[256];
	cry_strcpy(nameCache, GetName());
	char* s = strchr(nameCache, '(');
	if (s)
		s[0] = 0;
	string sNew;
	SShaderCombIdent Ident = pInst->m_Ident;
	Ident.m_GLMask = m_nMaskGenFX;
	gRenDev->m_cEF.mfInsertNewCombination(Ident, pInst->m_eClass, nameCache, 0, &sNew, 0);

	if (CRenderer::CV_r_shaderslogcachemisses > 1 && !gRenDev->m_bShaderCacheGen && !gEnv->IsEditor())
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, "[SHADERS] GCM Global Cache Miss: %s\n", sNew.c_str());
	}

	string sEntry;

	sEntry = "[";
	sEntry += CParserBin::GetPlatformShaderlistName();
	sEntry += "]";
	sEntry += sNew;

	CCryNameTSCRC cryName(sEntry);

	// do we already contain this entry (vec is sorted so lower bound gives us already the good position to insert)
	CShaderMan::ShaderCacheMissesVec::iterator it = std::lower_bound(Man.m_ShaderCacheMisses.begin(), Man.m_ShaderCacheMisses.end(), cryName);
	if (it == Man.m_ShaderCacheMisses.end() || cryName != (*it))
	{
		Man.m_ShaderCacheMisses.insert(it, cryName);

		if (CRenderer::CV_r_shaderslogcachemisses)
		{
			FILE* pFile = fopen(Man.m_ShaderCacheMissPath.c_str(), "a+");
			if (pFile)
			{
				fprintf(pFile, "%s\n", sEntry.c_str());
				fclose(pFile);
			}
		}

		// call callback if provided to inform client about misses
		if (Man.m_ShaderCacheMissCallback)
		{
			(*Man.m_ShaderCacheMissCallback)(sEntry.c_str());
		}
	}

#if ENABLE_STATOSCOPE
	if (gEnv->pStatoscope)
	{
		gEnv->pStatoscope->AddUserMarker("Shaders/Global Cache Misses", sNew.c_str());
	}
#endif
}

void CHWShader_D3D::mfLogShaderRequest(SHWSInstance* pInst)
{
#if !defined(_RELEASE)
	IF (CRenderer::CV_r_shaderssubmitrequestline > 1, 0)
		mfSubmitRequestLine(pInst);
#endif
}
