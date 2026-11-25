#include "tf.h"
#include "unit.inc"
#include "integ.inc"
#include "system.inc"
#include "exp.inc"

int main(void){
    char p[96]; snprintf(p, sizeof p, "out/metrics_%s.csv", TAG); mf = fopen(p, "w"); if (mf) fprintf(mf, "group,key,value\n");
    unit_tests(); integ_tests(); system_tests(); experiments();
    snprintf(p, sizeof p, "out/results_%s.csv", TAG); FILE *f = fopen(p, "w");
    if (f) { fprintf(f, "id,level,req,description,expected,measured,result\n");
        for (int i = 0; i < ntc; i++) { tc_t *t = &tcs[i]; fprintf(f, "%s,%s,\"%s\",\"%s\",\"%s\",\"%s\",%s\n", t->id, t->lvl, t->req, t->desc, t->exp, t->meas, t->pass ? "PASS" : "FAIL"); } fclose(f); }
    if (mf) fclose(mf);
    printf("\nTAG=%s  %d/%d passed\n", TAG, npass, ntc);
    return 0;
}
