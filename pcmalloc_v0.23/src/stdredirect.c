#include <stdio.h>

void
gamess0_stdin_redirect_(){
	freopen("cytosine.2.config","r", stdin);
}

void
gamess1_stdin_redirect_(){
	freopen("h2ocu2+.gradient.config","r", stdin);
}

void
gamess2_stdin_redirect_(){
	freopen("triazolium.config","r", stdin);
}

void
milc_stdin_redirect_(){
	freopen("su3imp.in","r", stdin);
}

void
leslie3d_stdin_redirect_(){
	freopen("leslie3d.in","r", stdin);
}

void
gobmk0_stdin_redirect_(){
	freopen("13x13.tst","r", stdin);
}

void
gobmk1_stdin_redirect_(){
	freopen("nngs.tst","r", stdin);
}

void
gobmk2_stdin_redirect_(){
	freopen("score2.tst","r", stdin);
}

void
gobmk3_stdin_redirect_(){
	freopen("trevorc.tst","r", stdin);
}

void
gobmk4_stdin_redirect_(){
	freopen("trevord.tst","r", stdin);
}


