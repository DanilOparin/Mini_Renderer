#include "pch.h"
#include "ModelLoaderApi.h"

#include <fbxsdk.h>


namespace Engine {


	FbxVector4 GetNormal(FbxGeometryElementNormal* normal, const UINT32 faceidx) {
		if (normal->GetMappingMode() == FbxGeometryElement::eByPolygonVertex) {
			if (normal->GetReferenceMode() == FbxGeometryElement::eDirect) {
				return normal->GetDirectArray().GetAt(faceidx);
			}
			const int idx = normal->GetIndexArray().GetAt(faceidx);
			return normal->GetDirectArray().GetAt(idx);

		}
		else {
			PRINT_N("Not implemented normal handling for other than face indices");
		}

		return FbxVector4();
	}

	bool CompareVertex(Render::Vertex& a, Render::Vertex& b)
	{
		return
			a.position.x == b.position.x &&
			a.position.y == b.position.y &&
			a.position.z == b.position.z &&

			a.normal.x == b.normal.x &&
			a.normal.y == b.normal.y &&
			a.normal.z == b.normal.z &&

			a.uv.x == b.uv.x &&
			a.uv.y == b.uv.y;
	}

	DirectX::XMFLOAT2 GetVertUV(FbxMesh* pMesh,int uvSetIndex,int vertIndex,int polyIndex)
	{
		FbxStringList uvSetNames;
		pMesh->GetUVSetNames(uvSetNames);

		if (uvSetNames.GetCount() == 0)
			return { 0.0f, 0.0f };

		const char* uvSetName =
			uvSetNames.GetStringAt(uvSetIndex);

		const FbxGeometryElementUV* uvElement =
			pMesh->GetElementUV(uvSetName);

		if (!uvElement)
			return { 0.0f, 0.0f };

		FbxVector2 uv;

		switch (uvElement->GetMappingMode())
		{
		case FbxGeometryElement::eByControlPoint:
		{
			int controlPointIndex =
				pMesh->GetPolygonVertex(
					polyIndex,
					vertIndex);

			int uvIndex =
				(uvElement->GetReferenceMode() ==
					FbxGeometryElement::eDirect)
				?
				controlPointIndex
				:
				uvElement->GetIndexArray()
				.GetAt(controlPointIndex);

			uv =
				uvElement->GetDirectArray()
				.GetAt(uvIndex);

			break;
		}

		case FbxGeometryElement::eByPolygonVertex:
		{
			FbxVector2 uvValue;
			bool unmapped;

			pMesh->GetPolygonVertexUV(
				polyIndex,
				vertIndex,
				uvSetName,
				uvValue,
				unmapped);

			uv = uvValue;

			break;
		}

		default:
			return { 0.0f, 0.0f };
		}

		return DirectX::XMFLOAT2(
			(float)uv[0],
			1.0f - (float)uv[1]); // FBX -> DirectX
	}


	void ModelLoaderApi::LoadFBXModel(const char* filepath, std::vector<Render::Vertex>& outVertices, std::vector<UINT32>& outIndices, std::vector<Render::MeshDrawData>& outMeshes)
	{

		FbxManager* manager = FbxManager::Create();

		FbxIOSettings* iosettings = FbxIOSettings::Create(manager, IOSROOT);
		manager->SetIOSettings(iosettings);

		FbxImporter* importer = FbxImporter::Create(manager, "FBX importer");

		if (!importer->Initialize(filepath, -1, manager->GetIOSettings())) {
			PRINT_N("Model loading failed");
			PRINT_N("Error code was: " << importer->GetStatus().GetErrorString());
		}
		else {
			PRINT_N("SUCCES: LOADED MODEL AT " << filepath);

		}

		FbxScene* scene = FbxScene::Create(manager, "ModelLoadingScene");

		importer->Import(scene);

		importer->Destroy();


		std::vector<Render::Vertex> totalvertices;
		std::vector<UINT32> totalindices;

		size_t vertexoffset = 0;
		size_t indexoffset = 0;


		for (int geometry = 0; geometry < scene->GetGeometryCount(); geometry++) {

			PRINT_N("Geometry #" << geometry+1);

			FbxMesh* mesh = static_cast<FbxMesh*>(scene->GetGeometry(geometry));


			std::vector<Render::Vertex> meshvertices;
			std::vector<UINT32> meshindices;

			const int facecount = mesh->GetPolygonCount();
			FbxVector4* controlpoints = mesh->GetControlPoints();
			const int contronpointcount = mesh->GetControlPointsCount();

			unsigned int vtx = 0;

			for (int face = 0; face < facecount; face++) {
				const int facervertexcount = mesh->GetPolygonSize(face);

				for (int facevtx = 0; facevtx < facervertexcount; facevtx++) {

					const int controlpointidx = mesh->GetPolygonVertex(face, facevtx);

					Render::Vertex vertex;
					const FbxVector4& positondata = controlpoints[controlpointidx];

					vertex.position = { (float)positondata[0],(float)positondata[1],(float)positondata[2] };


					const int normalcount = mesh->GetElementNormalCount();

					for (int normalelem = 0; normalelem < normalcount; normalelem++) {
						FbxGeometryElementNormal* pnormal = mesh->GetElementNormal(normalelem);

						const FbxVector4& normaldata = GetNormal(pnormal, vtx);

						vertex.normal = { (float)normaldata[0],(float)normaldata[1],(float)normaldata[2] };
					}

					vertex.uv = GetVertUV(mesh, 0, facevtx, face);

					std::cout
						<< "Face: " << face
						<< " Vert: " << facevtx
						<< " UV: "
						<< vertex.uv.x
						<< " "
						<< vertex.uv.y
						<< std::endl;

					meshvertices.push_back(vertex);

					meshindices.push_back(
						(UINT32)meshvertices.size() - 1);

					vtx++;
				}
			
			}
			

			Render::MeshDrawData meshdrawdata;

			meshdrawdata.vertexoffset = vertexoffset;
			meshdrawdata.vertexcount = meshvertices.size();
			meshdrawdata.indexoffset = indexoffset;
			meshdrawdata.indexcount = meshindices.size();
			
		
			outMeshes.emplace_back(meshdrawdata);


			vertexoffset += meshdrawdata.vertexcount;
			indexoffset += meshdrawdata.indexcount;

			totalvertices.insert(totalvertices.end(), meshvertices.begin(), meshvertices.end());
			totalindices.insert(totalindices.end(), meshindices.begin(), meshindices.end());

			PRINT_N("Loaded model!");
			
		}

		outVertices = std::move(totalvertices);
		outIndices = std::move(totalindices);

	}

}






