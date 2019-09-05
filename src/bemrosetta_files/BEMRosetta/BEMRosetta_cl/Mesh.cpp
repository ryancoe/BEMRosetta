#include "BEMRosetta.h"

	
String MeshData::Load(String fileName, double rho, double g) {
	String ext = ToLower(GetFileExt(fileName));
	String ret;
	bool y0z = false, x0z = false;
	if (ext == ".dat") {
		ret = LoadDatNemoh(fileName, x0z);
		if (!ret.IsEmpty()) 
			ret = LoadDatWamit(fileName);
		else
			ret = String();
	} else if (ext == ".gdf") 
		ret = LoadGdfWamit(fileName, y0z, x0z); 
	else if (ext == ".stl") {
		bool isText;
		ret = LoadStlTxt(fileName, isText);
		if (!ret.IsEmpty() && !isText) 
			ret = LoadStlBin(fileName);
		else
			ret = String(); 
	} else
		return t_("Unknown file extension");	
	
	if (!ret.IsEmpty())
		return ret;
	
	if (y0z)
		mesh.DeployXSymmetry();
	if (x0z)
		mesh.DeployYSymmetry();	
	
	cg = cg0 = Point3D(0, 0, 0);
	
	mesh.nodes = clone(mesh.nodes0);
	
	AfterLoad(rho, g, false);
	
	return String();
}

void MeshData::SaveAs(String file, MESH_FMT type, double g, bool meshAll, bool positionOriginal) {
	const Vector<Panel> &panels = meshAll ? mesh.panels : under.panels;
	const Vector<Point3D> &nodes = positionOriginal ? mesh.nodes0 : under.nodes;
	
	if (panels.IsEmpty())
		throw Exc(t_("Model is empty. No panels found"));
		
	if (type == UNKNOWN) {
		String ext = ToLower(GetFileExt(file));
		
		if (ext == ".gdf")
			type = MeshData::WAMIT_GDF;
		else if (ext == ".dat")
			type = MeshData::NEMOH_DAT;
		else if (ext == ".stl")
			type = MeshData::STL_TXT;
		else
			throw Exc(Format(t_("Conversion to type of file '%s' not supported"), file));
	}
	
	bool y0z = false, x0z = false;	// Pending
	
	if (type == WAMIT_GDF) 
		SaveGdfWamit(file, panels, nodes, g, y0z, x0z);	
	else if (type == NEMOH_DAT) 
		SaveDatNemoh(file, panels, nodes, x0z);
	else if (type == STL_BIN)		
		SaveStlBin(file, panels, nodes);
	else if (type == STL_TXT)		
		SaveStlTxt(file, panels, nodes);
	else
		throw Exc(t_("Unknown mesh file type"));
}

String MeshData::Heal(Function <void(String, int pos)> Status) {
	String ret = mesh.Heal(Status);
	if (!ret.IsEmpty())
		return ret;
	
	return String();
}
		
void MeshData::AfterLoad(double rho, double g, bool onlyCG) {
	if (!onlyCG) {
		mesh.GetPanelParams();
		mesh.GetLimits();
		mesh.GetSurface();
		mesh.GetVolume();
		
		under.Underwater(mesh);
		under.GetPanelParams();
		waterPlaneArea = under.GetWaterPlaneArea();
		under.GetSurface();
		under.GetVolume();
		
		cb = under.GetCenterOfBuoyancy();
	}
	under.GetHydrostaticStiffness(c, cb, rho, cg, Null, g, 0);
}

void MeshData::Report() {
	BEMData::Print("\n\n" + Format(t_("Loaded mesh '%s'"), file));
	
	BEMData::Print(x_("\n") + Format(t_("Mesh limits (%f - %f, %f - %f, %f - %f)"), 
			mesh.env.minX, mesh.env.maxX, mesh.env.minY, mesh.env.maxY, mesh.env.minZ, mesh.env.maxZ));
	BEMData::Print(x_("\n") + Format(t_("Mesh water-plane area %f"), waterPlaneArea));
	BEMData::Print(x_("\n") + Format(t_("Mesh surface %f"), mesh.surface));
	BEMData::Print(x_("\n") + Format(t_("Mesh volume %f"), mesh.volume));
	BEMData::Print(x_("\n") + Format(t_("Mesh underwater surface %f"), under.surface));
	BEMData::Print(x_("\n") + Format(t_("Mesh underwater volume %f"), under.volume));
	BEMData::Print(x_("\n") + Format(t_("Center of buoyancy (%f, %f, %f)"), cb.x, cb.y, cb.z));
	
	BEMData::Print(x_("\n") + Format(t_("Loaded %d panels and %d nodes"), mesh.panels.GetCount(), mesh.nodes.GetCount()));
}