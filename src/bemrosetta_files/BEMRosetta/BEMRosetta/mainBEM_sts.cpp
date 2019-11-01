#include <CtrlLib/CtrlLib.h>
#include <Controls4U/Controls4U.h>
#include <ScatterCtrl/ScatterCtrl.h>
#include <GLCanvas/GLCanvas.h>
#include <RasterPlayer/RasterPlayer.h>
#include <CtrlScroll/CtrlScroll.h>

#include <BEMRosetta/BEMRosetta_cl/BEMRosetta.h>

using namespace Upp;

#include "main.h"


void MainStateSpace::Init() {
	CtrlLayout(*this);
	
	selTab = 0;
	isFilling = false;
	tab.WhenSet = [&] {
		LOGTAB(tab);
		if (!isFilling)
			selTab = tab.Get();
	};
}

void MainStateSpace::Clear() {
	tab.Reset();
	selTab = 0;
}

bool MainStateSpace::Load(BEMData &bem) {
	try {
		Upp::Array<HydroClass> &hydro = bem.hydros; 
		if (hydro.IsEmpty())
			return false;
		isFilling = true;
		tab.Reset();
		int sdof = 6*bem.Nb;
		
		plots.SetCount(sdof);
		for (int i = 0; i < sdof; ++i) {
			plots[i].SetCount(sdof);
			for (int j = 0; j < sdof; ++j) {
				if (!bem.onlyDiagonal || i == j) {
					plots[i][j].Init(i, j);
					if (plots[i][j].Load(hydro)) {
						if (i != j)
							tab.Add(plots[i][j].SizePos(), Hydro::StrBDOF(i, j));
						else
							tab.Add(plots[i][j].SizePos(), Hydro::StrBDOF(i));
					}
				}
			}
		}
		
		isFilling = false;
		if (tab.GetCount() == 0)
			return false;
		else if (tab.GetCount() > selTab)	
			tab.Set(selTab);
		return true;
	} catch (Exc e) {
		Exclamation(DeQtfLf(e));
		return false;
	}
}

void MainStateSpacePlot::Init(int _idof, int _jdof) {
	mainPlot.Init(_idof, _jdof, DATA_STS2);
	
	splitterTab.Horz(tab.SizePos(), mainPlot.SizePos());
	Add(splitterTab.SizePos());
}

bool MainStateSpacePlot::Load(Upp::Array<HydroClass> &hydro) {
	if (!mainPlot.Load(hydro))
		return false;

	tab.Reset();
	arrays.Clear();
	
	bool loaded = false;
	int idof = mainPlot.idof;
	int jdof = mainPlot.jdof;
	for (int id = 0; id < hydro.GetCount(); ++id) {
		Hydro &hy = hydro[id].hd();
		if (hy.IsLoadedStateSpace()) {
			Hydro::StateSpace &sts = hy.sts[idof][jdof];
			int row = 0;
			if (sts.A_ss.size() > 0 || sts.B_ss.size() > 0 || sts.C_ss.size() > 0) {
				loaded = true;
				ArrayCtrl &array = arrays.Add();
				InitArray(array);
				tab.Add(array.SizePos(), hy.name);
				if (sts.A_ss.size() > 0) {
					if (sts.A_ss.cols() > array.GetColumnCount()) {
						int ncols = static_cast<int>(sts.A_ss.cols()) - array.GetColumnCount();
						for (int i = 0; i < ncols; ++i)
							array.AddColumn("", 80);
					}
					array.Set(row++, 0, AttrText(t_("A_ss")).Bold());
					for (int r = 0; r < sts.A_ss.rows(); ++r)	{		
						for (int c = 0; c < sts.A_ss.cols(); ++c)
							array.Set(row + r, c, sts.A_ss(r, c));
					}
					row += static_cast<int>(sts.A_ss.rows());
				}
				if (sts.B_ss.size() > 0) {
					array.Set(row++, 0, AttrText(t_("B_ss")).Bold());
					for (int r = 0; r < sts.B_ss.size(); ++r)		
						array.Set(row, r, sts.B_ss(r));
					row++;
				}
				if (sts.C_ss.size() > 0) {
					array.Set(row++, 0, AttrText(t_("C_ss")).Bold());
					for (int c = 0; c < sts.C_ss.size(); ++c)			
						array.Set(row, c, sts.C_ss(c));
					row++;
				}
				if (sts.ssFrequencies.size() > 0) {
					array.Set(row++, 0, AttrText(t_("Frequencies")).Bold());
					for (int c = 0; c < sts.ssFrequencies.size(); ++c)			
						array.Set(row, c, sts.ssFrequencies[c]);
					row++;
				}
				if (sts.ssFreqRange.size() > 0) {
					array.Set(row++, 0, AttrText(t_("FreqRange")).Bold());
					for (int c = 0; c < sts.ssFreqRange.size(); ++c)			
						array.Set(row, c, sts.ssFreqRange[c]);
					row++;
				}					
				if (!IsNull(sts.ssMAE)) {
					array.Set(row++, 0, AttrText(t_("MAPE [%]")).Bold());
					array.Set(row++, 0, sts.ssMAE*100);
				}
			}
		}
	}
	return loaded;
}

void MainStateSpacePlot::InitArray(ArrayCtrl &array) {
	array.Reset();
	array.NoHeader().SetLineCy(EditField::GetStdHeight()).HeaderObject().Absolute();
	array.MultiSelect().SpanWideCells();
	array.WhenBar = [&](Bar &menu) {ArrayCtrlWhenBar(menu, array);};
}