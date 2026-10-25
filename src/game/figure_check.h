#pragma once

// For the launcher (Core of Light #34): which Imaginators toy files this game accepts, checked by
// the game's own code (figure_check.cpp). Started with --check_figures=<list>, the game checks the
// files named in that text file and writes <list>.result, then quits.

namespace rex::memory {
class Memory;
}

namespace imaginatorsrecomp {

// With --check_figures: checks, writes the result file and quits; otherwise nothing. Needs the
// game's image loaded (ReXApp::OnPostLoadXexImage), nothing of it has run yet.
void CheckFiguresIfRequested(rex::memory::Memory* memory);

}  // namespace imaginatorsrecomp
