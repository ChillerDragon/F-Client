/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/storage.h>
#include <game/client/component.h>
#include <game/mapitems.h>

#include "mapimages.h"

#include <engine/serverbrowser.h>

CMapImages::CMapImages()
{
	m_Info[MAP_TYPE_GAME].m_Count = 0;
	m_Info[MAP_TYPE_MENU].m_Count = 0;

	m_EasterIsLoaded = false;
	m_EntitiesIsLoaded = false;
}

void CMapImages::LoadMapImages(IMap *pMap, class CLayers *pLayers, int MapType)
{
	if(MapType < 0 || MapType >= NUM_MAP_TYPES)
		return;

	// unload all textures
	for(int i = 0; i < m_Info[MapType].m_Count; i++)
		Graphics()->UnloadTexture(&(m_Info[MapType].m_aTextures[i]));
	m_Info[MapType].m_Count = 0;

	int Start;
	pMap->GetType(MAPITEMTYPE_IMAGE, &Start, &m_Info[MapType].m_Count);
	m_Info[MapType].m_Count = clamp(m_Info[MapType].m_Count, 0, int(MAX_TEXTURES));

	// load new textures
	for(int i = 0; i < m_Info[MapType].m_Count; i++)
	{
		int TextureFlags = 0;
		bool FoundQuadLayer = false;
		bool FoundTileLayer = false;
		for(int k = 0; k < pLayers->NumLayers(); k++)
		{
			const CMapItemLayer * const pLayer = pLayers->GetLayer(k);
			if(!FoundQuadLayer && pLayer->m_Type == LAYERTYPE_QUADS && ((const CMapItemLayerQuads *)pLayer)->m_Image == i)
				FoundQuadLayer = true;
			if(!FoundTileLayer && pLayer->m_Type == LAYERTYPE_TILES && ((const CMapItemLayerTilemap *)pLayer)->m_Image == i)
				FoundTileLayer = true;
		}
		if(FoundTileLayer)
			TextureFlags = FoundQuadLayer ? IGraphics::TEXLOAD_MULTI_DIMENSION : IGraphics::TEXLOAD_ARRAY_256;

		CMapItemImage *pImg = (CMapItemImage *)pMap->GetItem(Start+i, 0, 0);
		if(pImg->m_External || (pImg->m_Version > 1 && pImg->m_Format != CImageInfo::FORMAT_RGB && pImg->m_Format != CImageInfo::FORMAT_RGBA))
		{
			char Buf[IO_MAX_PATH_LENGTH];
			char *pName = (char *)pMap->GetData(pImg->m_ImageName);
			str_format(Buf, sizeof(Buf), "mapres/%s.png", pName);
			m_Info[MapType].m_aTextures[i] = Graphics()->LoadTexture(Buf, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, TextureFlags);
		}
		else
		{
			void *pData = pMap->GetData(pImg->m_ImageData);
			m_Info[MapType].m_aTextures[i] = Graphics()->LoadTextureRaw(pImg->m_Width, pImg->m_Height, pImg->m_Version == 1 ? CImageInfo::FORMAT_RGBA : pImg->m_Format, pData, CImageInfo::FORMAT_RGBA, TextureFlags);
			pMap->UnloadData(pImg->m_ImageData);
		}
	}

	// easter time, preload easter tileset
	if(m_pClient->IsEaster())
		GetEasterTexture();
}

void CMapImages::OnMapLoad()
{
	LoadMapImages(Kernel()->RequestInterface<IMap>(), Layers(), MAP_TYPE_GAME);
}

void CMapImages::OnMenuMapLoad(IMap *pMap)
{
	CLayers MenuLayers;
	MenuLayers.Init(Kernel(), pMap);
	LoadMapImages(pMap, &MenuLayers, MAP_TYPE_MENU);
}

IGraphics::CTextureHandle CMapImages::GetEasterTexture()
{
	if(!m_EasterIsLoaded)
	{
		m_EasterTexture = Graphics()->LoadTexture("mapres/easter.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_ARRAY_256);
		if(!m_EasterTexture.IsValid())
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "mapimages", "Failed to load easter.png");
		m_EasterIsLoaded = true;
	}
	return m_EasterTexture;
}

IGraphics::CTextureHandle CMapImages::Get(int Index) const
{
	if(Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return m_Info[MAP_TYPE_GAME].m_aTextures[clamp(Index, 0, m_Info[MAP_TYPE_GAME].m_Count)];
	return m_Info[MAP_TYPE_MENU].m_aTextures[clamp(Index, 0, m_Info[MAP_TYPE_MENU].m_Count)];
}

int CMapImages::Num() const
{
	if(Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return m_Info[MAP_TYPE_GAME].m_Count;
	return m_Info[MAP_TYPE_MENU].m_Count;
}

void CMapImages::LoadBackground(class IMap *pMap)
{
	// unload all textures
	for(int i = 0; i < m_Info[MAP_TYPE_GAME].m_Count; i++)
		Graphics()->UnloadTexture(&(m_Info[MAP_TYPE_GAME].m_aTextures[i]));
	m_Info[MAP_TYPE_GAME].m_Count = 0;

	int Start;
	pMap->GetType(MAPITEMTYPE_IMAGE, &Start, &m_Info[MAP_TYPE_GAME].m_Count);

	// load new textures
	for(int i = 0; i < m_Info[MAP_TYPE_GAME].m_Count; i++)
	{
		CMapItemImage *pImg = (CMapItemImage *)pMap->GetItem(Start+i, 0, 0);
		if(pImg->m_External)
		{
			char Buf[256];
			char *pName = (char *)pMap->GetData(pImg->m_ImageName);
			str_format(Buf, sizeof(Buf), "mapres/%s.png", pName);
			m_Info[MAP_TYPE_GAME].m_aTextures[i] = Graphics()->LoadTexture(Buf, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
		}
		else
		{
			void *pData = pMap->GetData(pImg->m_ImageData);
			m_Info[MAP_TYPE_GAME].m_aTextures[i] = Graphics()->LoadTextureRaw(pImg->m_Width, pImg->m_Height, CImageInfo::FORMAT_RGBA, pData, CImageInfo::FORMAT_RGBA, 0);
			pMap->UnloadData(pImg->m_ImageData);
		}
	}
}

IGraphics::CTextureHandle CMapImages::GetEntities()
{
	CServerInfo Info;
	Client()->GetServerInfo(&Info);

	// DDNet default to prevent delay in seeing entities
	const char *pEntities = "ddnet";
	if (IsFDDrace(&Info))
		pEntities = "f-ddrace";
	else if(IsDDNet(&Info))
		pEntities = "ddnet";
	// commented out because fokkonaut ported DDNet server to 0.7 and we basically
	// dont have any old ddrace servers, only the ones that use DDNet entities
	else if(IsDDRace(&Info))
		pEntities = "ddnet"; // pEntities = "ddrace";
	else if(IsRace(&Info))
		pEntities = "race";
	else if(IsFNG(&Info))
		pEntities = "fng";
	else if(IsVanilla(&Info))
		pEntities = "vanilla";

	if (!m_EntitiesIsLoaded || m_pEntitiesGameType != pEntities)
	{
		char aPath[64];
		str_format(aPath, sizeof(aPath), "editor/entities_clear/%s.png", pEntities);
		
		if (m_EntitiesTextures.IsValid())
			Graphics()->UnloadTexture(&m_EntitiesTextures);
		m_EntitiesTextures = Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_MULTI_DIMENSION);
		m_EntitiesIsLoaded = true;
		m_pEntitiesGameType = pEntities;
	}
	return m_EntitiesTextures;
}