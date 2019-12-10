#include "BEMRosetta.h"
#include "data.brc"
#include <ScatterDraw/Unpedantic.h>
#include <plugin/bz2/bz2.h>
#include <plugin/lzma/lzma.h>
#include <plugin/lz4/lz4.h>
#include <plugin/zstd/zstd.h>
#include <ScatterDraw/Pedantic.h>

bool Fast::Load(String file, double g) {
	hd().file = file;	
	hd().name = GetFileTitle(file);
	
	hd().g = g;
	
	try {
		String hydroFile;
		if (GetFileExt(file) != ".dat") 
			throw Exc("\n" + Format(t_("File '%s' is not of FAST type"), file));
			
		BEMData::Print("\n\n" + Format(t_("Loading '%s'"), file));
		if (!Load_HydroDyn()) 
			throw Exc("\n" + Format(t_("File '%s' not found"), file));

		hydroFile = AppendFileName(GetFileFolder(file), AppendFileName(hydroFolder, hd().name));
		hd().code = Hydro::FAST_WAMIT;
		
		if (!Wamit::Load(ForceExt(hydroFile, ".hst"))) 
			return false;
		
		if (IsNull(hd().Nb))
			return false;
		
		if (hd().Nb > 1)
			throw Exc(Format(t_("FAST does not support more than one body in file '%s'"), file));	
		if (hd().head.IsEmpty())
			throw Exc(t_("No wave headings found in Wamit file"));
		if (abs(hd().head[0]) != abs(hd().head[hd().head.GetCount()-1]))
			throw Exc(Format(t_("FAST requires simetric wave headings. .3 file headings found from %f to %f"), hd().head[0], hd().head[hd().head.GetCount()-1])); 
	
		String ssFile = ForceExt(hydroFile, ".ss");
		if (FileExists(ssFile)) {
			BEMData::Print("\n\n" + Format(t_("Loading '%s'"), file));
			if (!Load_SS(ssFile)) 
				throw Exc("\n" + Format(t_("File '%s' not found"), file));	
		}
	} catch (Exc e) {
		BEMData::PrintError("\nError: " + e);
		hd().lastError = e;
		return false;
	}
	
	return true;
}

bool Fast::Load_HydroDyn() {
	FileInLine in(hd().file);
	if (!in.IsOpen())
		return false;

	hd().Nb = WaveNDir = 1;
	hd().Vo.SetCount(hd().Nb, 0);
	hd().rho = hd().h = hd().len = WaveDirRange = Null;
	
	FieldSplit f(in);
	while (!in.IsEof()) {
		f.Load(in.GetLine());
		if (f.GetCount() == 0)
			break;
		if (f.GetText(1) == "WtrDens") 
			hd().rho = f.GetDouble(0);
		else if (f.GetText(1) == "WtrDpth") 
			hd().h = f.GetDouble(0);
		else if (f.GetText(1) == "WAMITULEN") 
			hd().len = f.GetDouble(0);
		else if (f.GetText(1) == "PtfmVol0") 
			hd().Vo[0] = f.GetDouble(0);
		else if (f.GetText(1) == "WaveNDir") 
			WaveNDir = f.GetInt(0);
		else if (f.GetText(1) == "WaveDirRange") 
			WaveDirRange = f.GetDouble(0);
		else if (f.GetText(1) == "PotFile") { 
			String path = f.GetText(0);
			path.Replace("\"", "");
			hydroFolder = GetFileFolder(path);
			hd().name = GetFileName(path);
		}
	}
	if (IsNull(hd().rho) || IsNull(hd().h) || IsNull(hd().len))
		throw Exc(Format(t_("Wrong format in FAST file '%s'"), hd().file));
	
	return true;
}


void Fast::Save(String file) {
	try {
		file = ForceExt(file, ".dat");
		Save_HydroDyn(file, true);
		String hydroFile = AppendFileName(AppendFileName(GetFileFolder(file), hydroFolder), hd().name);
		DirectoryCreate(AppendFileName(GetFileFolder(file), hydroFolder));
	
		Wamit::Save(hydroFile, true);
		
		if (hd().IsLoadedStateSpace()) {
			String fileSts = ForceExt(hydroFile, ".ss");
			BEMData::Print("\n- " + Format(t_("State Space file '%s'"), GetFileName(fileSts)));
			Save_SS(fileSts);
		}
	} catch (Exc e) {
		BEMData::PrintError(Format("\n%s: %s", t_("Error"), e));
		hd().lastError = e;
	}
}

void Fast::Save_HydroDyn(String fileName, bool force) {
	String strFile;
	
	if (hydroFolder.IsEmpty())
		hydroFolder = "HydroData";
	
	if (FileExists(fileName)) {
		double lVo = Null, lrho = Null, lh = Null, llen = Null, lWaveDirRange = Null;
		int lWaveNDir = Null;
		
		FileInLine in(fileName);
		if (!in.IsOpen())
			return;
	
		FieldSplit f(in);
		while (!in.IsEof()) {
			f.Load(in.GetLine());
			if (f.GetText(1) == "WtrDens") 
				lrho = f.GetDouble(0);
			else if (f.GetText(1) == "WtrDpth") 
				lh = f.GetDouble(0);
			else if (f.GetText(1) == "WAMITULEN") 
				llen = f.GetDouble(0);
			else if (f.GetText(1) == "PtfmVol0") 
				lVo = f.GetDouble(0);
			else if (f.GetText(1) == "WaveNDir") 
				lWaveNDir = f.GetInt(0);
			else if (f.GetText(1) == "WaveDirRange") 
				lWaveDirRange = f.GetDouble(0);
		}
		in.Close();
		
		if (hd().Nb != 1)
			throw Exc(t_("Number of bodies different to 1 incompatible with FAST"));
		if (IsNull(lVo))
			throw Exc(Format(t_("Volume (PtfmVol0) not found in FAST file '%s'"), fileName));
		if (IsNull(lrho))
			throw Exc(Format(t_("Density (WtrDens) not found in FAST file '%s'"), fileName));
		if (IsNull(lh))
			throw Exc(Format(t_("Water depth (WtrDpth) not found in FAST file '%s'"), fileName));
		if (IsNull(llen))
			throw Exc(Format(t_("Length scale (WAMITULEN) not found in FAST file '%s'"), fileName));
		if (IsNull(lWaveNDir))
			throw Exc(Format(t_("Number of wave directions (WaveNDir) not found in FAST file '%s'"), fileName));
		if (IsNull(lWaveDirRange))
			throw Exc(Format(t_("Range of wave directions (WaveDirRange) not found in FAST file '%s'"), fileName));
		
		strFile = LoadFile(fileName);
		int poslf, pos;
			
		if (!force) {
			if (lVo != hd().Vo[0])
				throw Exc(Format(t_("Different %s (%f != %f) in FAST file '%s'"), t_("volume"), hd().Vo[0], lVo, hd().file));
			if (lrho != hd().rho)
				throw Exc(Format(t_("Different %s (%f != %f) in FAST file '%s'"), t_("density"), hd().rho, lrho, hd().file));
			if (lh != hd().h)
				throw Exc(Format(t_("Different %s (%f != %f) in FAST file '%s'"), t_("water depth"), hd().h, lh, hd().file));
			if (llen != hd().len)
				throw Exc(Format(t_("Different %s (%f != %f) in FAST file '%s'"), t_("length scale"), hd().len, llen, hd().file));
			if (!IsNull(WaveNDir) && lWaveNDir != WaveNDir)
				throw Exc(Format(t_("Different %s (%d != %d) in FAST file '%s'"), t_("number of wave headings"), WaveNDir, lWaveNDir, hd().file));
			if (!IsNull(WaveDirRange) && lWaveDirRange != WaveDirRange)
				throw Exc(Format(t_("Different %s (%f != %f) in FAST file '%s'"), t_("headings range"), WaveDirRange, lWaveDirRange, hd().file));
		} else {		
			pos   = strFile.Find("WtrDens");
			poslf = strFile.ReverseFind("\n", pos);
			if (pos < 0 || poslf < 0)
				throw Exc(Format(t_("Bad format parsing FAST file '%s' for %s"), hd().file, "WtrDens"));
			strFile = strFile.Left(poslf+1) + Format("%14>f   ", hd().rho) + strFile.Mid(pos);
			pos   = strFile.Find("WtrDpth");
			poslf = strFile.ReverseFind("\n", pos);
			if (pos < 0 || poslf < 0)
				throw Exc(Format(t_("Bad format parsing FAST file '%s' for %s"), hd().file, "WtrDpth"));
			strFile = strFile.Left(poslf+1) + Format("%14>f   ", hd().h) + strFile.Mid(pos);
			pos   = strFile.Find("WAMITULEN");
			poslf = strFile.ReverseFind("\n", pos);
			if (pos < 0 || poslf < 0)
				throw Exc(Format(t_("Bad format parsing FAST file '%s' for %s"), hd().file, "WAMITULEN"));
			strFile = strFile.Left(poslf+1) + Format("%14>f   ", hd().len) + strFile.Mid(pos);
			pos   = strFile.Find("PtfmVol0");
			poslf = strFile.ReverseFind("\n", pos);
			if (pos < 0 || poslf < 0)
				throw Exc(Format(t_("Bad format parsing FAST file '%s' for %s"), hd().file, "PtfmVol0"));
			double hdVo0 = 0;
			if (hd().Vo.GetCount() > 0) 				
				hdVo0 = hd().Vo[0];
			strFile = strFile.Left(poslf+1) + Format("%14>f   ", hdVo0) + strFile.Mid(pos);
			pos   = strFile.Find("WaveNDir");
			poslf = strFile.ReverseFind("\n", pos);
			if (pos < 0 || poslf < 0)
				throw Exc(Format(t_("Bad format parsing FAST file '%s' for %s"), hd().file, "WaveNDir"));
			if (IsNull(WaveNDir))
				strFile = strFile.Left(poslf+1) + Format("%14>d   ", hd().Nh) + strFile.Mid(pos);
			else
				strFile = strFile.Left(poslf+1) + Format("%14>d   ", WaveNDir) + strFile.Mid(pos);
			pos   = strFile.Find("WaveDirRange");
			poslf = strFile.ReverseFind("\n", pos);
			if (pos < 0 || poslf < 0)
				throw Exc(Format(t_("Bad format parsing FAST file '%s' for %s"), hd().file, "WaveDirRange"));
			if (IsNull(WaveDirRange))
				strFile = strFile.Left(poslf+1) + Format("%14>f   ", (hd().head[hd().Nh-1] - hd().head[0])/2) + strFile.Mid(pos);
			else
				strFile = strFile.Left(poslf+1) + Format("%14>f   ", WaveDirRange) + strFile.Mid(pos);
		}
		pos   = strFile.Find("PotFile");
		poslf = strFile.ReverseFind("\n", pos);
		if (pos < 0 || poslf < 0)
			throw Exc(Format(t_("Bad format parsing FAST file '%s' for %s"), hd().file, "PotFile"));
		
		String folder = AppendFileName(hydroFolder, hd().name);
		strFile = strFile.Left(poslf+1) + Format("\"%s\" ", folder) + strFile.Mid(pos);
	} else {
		strFile = ZstdDecompress(hydroDyn, hydroDyn_length);
		
		String srho;
		if (IsNull(hd().rho))
			srho = FormatDouble(hd().GetBEMData().rho);
		else
			srho = FormatDouble(hd().rho);
		strFile.Replace("[WtrDens]", srho);
		String sh;
		if (IsNull(hd().h) || hd().h < 0)
			sh = "INFINITE";
		else
			sh = FormatDouble(hd().h);
		strFile.Replace("[WtrDpth]", sh);
		String slen;
		if (IsNull(hd().len))
			slen = "1";
		else
			slen = FormatDouble(hd().len);
		strFile.Replace("[WAMITULEN]", slen);
		double hdVo0 = 0;
		if (hd().Vo.GetCount() > 0) 				
			hdVo0 = hd().Vo[0];
		strFile.Replace("[PtfmVol0]", FormatDouble(hdVo0));
		if (IsNull(WaveNDir))
			strFile.Replace("[WaveNDir]", FormatInt(hd().Nh));
		else
			strFile.Replace("[WaveNDir]", FormatInt(WaveNDir));
		if (IsNull(WaveDirRange))
			strFile.Replace("[WaveDirRange]", FormatDouble((hd().head[hd().Nh-1] - hd().head[0])/2));
		else
			strFile.Replace("[WaveDirRange]", FormatDouble(WaveDirRange));
		strFile.Replace("[PotFile]", Format("\"%s\"", AppendFileName(hydroFolder, hd().name)));
	}
	if (!SaveFile(fileName, strFile))
		throw Exc(Format(t_("Imposible to save file '%s'"), hd().file));
}

// Just can save the first body			
void Fast::Save_SS(String fileName) {
	FileOut out(fileName);
	if (!out.IsOpen())
		throw Exc(Format(t_("Impossible to open '%s'"), fileName));
	
	if (hd().Nb > 1)
		BEMData::PrintWarning(S("\n") + t_(".ss format only allows to save one body. Only first body is saved"));	

	if (!hd().stsProcessor.IsEmpty())
		out << Format("BEMRosetta state space matrices obtained with %s", hd().stsProcessor) << "\n";
	else	
		out << "BEMRosetta state space matrices" << "\n";
	Eigen::Index nstates = 0;
	Vector<Eigen::Index> nstatesdof;
	for (int idf = 0; idf < 6; ++idf) {
		Eigen::Index num = 0;
		for (int jdf = 0; jdf < 6; ++jdf) {
			const Hydro::StateSpace &sts = hd().sts[idf][jdf];
			num += sts.A_ss.cols();
		}
		if (num > 0) 
			out << "1  ";
		else 
			out << "0  ";
		nstates += num;
		nstatesdof << num;
	}
	out << "  %Enabled DoFs\n";
	out << Format("%20<d", int(nstates)) << "%Radiation states\n";
	for (int i = 0; i < nstatesdof.GetCount(); ++i)
		out << Format("%3<d", int(nstatesdof[i]));
	out << "  %Radiation states per DOFs\n";	

	MatrixXd A, B, C;
	A.setConstant(nstates, nstates, 0);
	B.setConstant(nstates, 6, 0);
	C.setConstant(6, nstates, 0);
	int pos = 0;
	for (int jdf = 0; jdf < 6; ++jdf) {
		for (int idf = 0; idf < 6; ++idf) {
			const Hydro::StateSpace &sts = hd().sts[idf][jdf];
			if (sts.A_ss.size() > 0) {
				for (int r = 0; r < sts.A_ss.rows(); ++r) 
					for (int c = 0; c < sts.A_ss.cols(); ++c) {
						A(pos + r, pos + c) = sts.A_ss(r, c);
						B(pos + r, jdf)    = sts.B_ss(r);
						C(idf, pos + r)    = sts.C_ss(r);
					}
				pos += int(sts.A_ss.rows());
			}
		}
	}
	for (int r = 0; r < A.rows(); ++r) {
		for (int c = 0; c < A.cols(); ++c) 
			out << Format("%e ", A(r, c));
		out << "\n";
	}
	for (int r = 0; r < B.rows(); ++r) {
		for (int c = 0; c < B.cols(); ++c) 
			out << Format("%e ", B(r, c));
		out << "\n";
	}
	for (int r = 0; r < C.rows(); ++r) {
		for (int c = 0; c < C.cols(); ++c) 
			out << Format("%e ", -C(r, c));
		out << "\n";
	}
}

static bool CheckRepeated(const Vector<Eigen::Index> &list) {
	for (int i = 0; i < list.GetCount()-1; ++i) {
		for (int j = i+1; j < list.GetCount(); ++j) {
			if (list[i] == list[j])
				return true;
		}
	}
	return false;
}

static void SortBy(Vector<Eigen::Index> &list, int idf) {
	Sort(list);
	for (int i = 0; i < list.GetCount(); ++i) {
		if (list[i] == idf) {
			list.Remove(i);
			return;
		}
	}
}

static bool FillDOF(const MatrixXd &C, int idf, int pos, int nstates, Vector<Eigen::Index> &listdof, Eigen::Index &nlistdof) {
	for (int ndofdof = 1; ndofdof <= 6; ++ndofdof) {
		if (nstates%ndofdof != 0)
			continue;
		int delta = nstates/ndofdof;
		for (int kk = 0; kk < ndofdof; ++kk) {			// Horizontal, left -> right
			int idfOK = -1;
			for (int idff = 0; idff < 6; ++idff) {		// Vertical, top -> bottom
				double sum = 0;
				for (int i = 0; i < delta; ++i)			// Horizontal, left -> right
					sum += C(idff, pos+kk*delta+i);
				if (sum != 0) {
					if (idfOK >= 0) {
						idfOK = -1;
						break;
					}
					idfOK = idff;
				}
			}
			if (idfOK < 0)
				break;
			listdof << idfOK;
			nlistdof = delta;
		}
		if (!listdof.IsEmpty()) {
			if (!CheckRepeated(listdof)) {	
				SortBy(listdof, idf);
				return true;		
			}
		}
		listdof.Clear();
	}
	return false;
}			

bool Fast::Load_SS(String fileName) {
	FileInLine in(fileName);
	if (!in.IsOpen())
		return false;

	if (hd().Nb > 1)
		BEMData::PrintWarning(S("\n") + t_(".ss format only allows to save one body. Only first body is retrieved"));
	
	String line; 
	FieldSplit f(in);
	
	hd().stsProcessor = TrimBoth(in.GetLine());
	hd().stsProcessor.Replace("BEMRosetta state space matrices obtained with ", "");
	
	hd().InitializeSts();
	
	in.GetLine();
	
	f.Load(in.GetLine());
	int nstates = f.GetInt(0);
	
	Vector<Eigen::Index> nstatesdof;
	f.Load(in.GetLine());
	int numtot = 0;
	for (int i = 0; i < 6; ++i) {
		int num = f.GetInt(i);
		nstatesdof << num;
		numtot += num;
	}
	if (numtot != nstates)
		throw Exc(Format(t_("Sum of states %d does no match total radiation states %d"), numtot, nstates));
	
	MatrixXd A, B, C;
	A.setConstant(nstates, nstates, Null);
	B.setConstant(nstates, 6, Null);
	C.setConstant(6, nstates, Null);
	
	for (int r = 0; r < A.rows(); ++r) {
		f.Load(in.GetLine());
		for (int c = 0; c < A.cols(); ++c) 
			A(r, c) = f.GetDouble(c);
	}
	for (int r = 0; r < B.rows(); ++r) {
		f.Load(in.GetLine());
		for (int c = 0; c < B.cols(); ++c) 
			B(r, c) = f.GetDouble(c);
	}
	for (int r = 0; r < C.rows(); ++r) {
		f.Load(in.GetLine());
		for (int c = 0; c < C.cols(); ++c) 
			C(r, c) = -f.GetDouble(c);
	}	
	Vector<Vector<Eigen::Index>> dofdof;
	Vector<Eigen::Index> ndofdof;
	dofdof.SetCount(nstatesdof.GetCount());
	ndofdof.SetCount(nstatesdof.GetCount());
	int pos0 = 0;
	for (int idf = 0; idf < dofdof.GetCount(); ++idf) {
		if (!FillDOF(C, idf, pos0, int(nstatesdof[idf]), dofdof[idf], ndofdof[idf]))
			throw Exc(Format(t_("Unknown structure in C matrix (%d, %d, %d)"), idf, pos0, int(nstatesdof[idf])));
		dofdof[idf].Insert(0, idf);
		pos0 += int(nstatesdof[idf]);
	}
	Eigen::Index pos = 0;
	for (int idf = 0; idf < dofdof.GetCount(); ++idf) {
		for (int i = 0; i < dofdof[idf].GetCount(); ++i) {
			int jdf = int(dofdof[idf][i]);
			Hydro::StateSpace &sts = hd().sts[idf][jdf];
			Eigen::Index num = ndofdof[idf];
			sts.A_ss.setConstant(num, num, Null);
			sts.B_ss.setConstant(num, Null);
			sts.C_ss.setConstant(num, Null);
			for (int r = 0; r < sts.A_ss.rows(); ++r) {
				for (int c = 0; c < sts.A_ss.cols(); ++c) {
					sts.A_ss(r, c) = A(pos + r, pos + c);
					sts.B_ss(r)    = B(pos + r, jdf);
					sts.C_ss(r)    = C(jdf, pos + r);
				}		 
			}
			sts.GetTFS(hd().w);
			pos += num;
		}
	}
	return true;
}