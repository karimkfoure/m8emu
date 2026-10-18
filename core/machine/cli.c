#define _POSIX_C_SOURCE 200809L
#include <m8emu/m8rt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9; }
static void drain(m8rt *rt,const char *path,int pcm) {
    if(!path) return;
    FILE *f=fopen(path,"wb");if(!f) { perror(path);return; }
    unsigned char b[65536];size_t n;
    while((n=pcm?m8rt_read_pcm(rt,b,sizeof b):m8rt_read_cdc(rt,b,sizeof b))) fwrite(b,1,n,f);
    fclose(f);
}
int main(int argc,char **argv) {
    const char *firmware=NULL,*image=NULL,*pcm=NULL,*cdc=NULL,*profile=NULL;
    uint64_t iters=10000;uint32_t wall_us=30000000;m8rt_options options={0,1,1,0};
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--help")) { puts("m8rt --firmware HEX|BIN [--image IMG] [--iters N] [--seconds N] [--reference] [--pcm FILE] [--cdc FILE] [--profile FILE]");return 0; }
        if(!strcmp(argv[i],"--reference")) { options.reference_policy=1;continue; }
        if(i+1>=argc) { fprintf(stderr,"missing argument: %s\n",argv[i]);return 2; }
        const char *key=argv[i],*value=argv[++i];
        if(!strcmp(key,"--firmware")) firmware=value;else if(!strcmp(key,"--image")) image=value;
        else if(!strcmp(key,"--pcm")) pcm=value;else if(!strcmp(key,"--cdc")) cdc=value;
        else if(!strcmp(key,"--profile")) { profile=value;options.profile_blocks=1; }
        else if(!strcmp(key,"--iters")) iters=strtoull(value,NULL,0);
        else if(!strcmp(key,"--seconds")) wall_us=(uint32_t)(atof(value)*1e6);
        else { fprintf(stderr,"unknown option: %s\n",key);return 2; }
    }
    if(!firmware) { fprintf(stderr,"--firmware is required\n");return 2; }
    m8rt *rt=m8rt_create(firmware,image,&options);
    if(!rt || *m8rt_error(rt)) { fprintf(stderr,"%s\n",m8rt_error(rt));m8rt_destroy(rt);return 1; }
    double start=now();int rc=m8rt_run(rt,iters,0,wall_us,0);double wall=now()-start;
    printf("iters=%llu cycles=%llu pc=0x%08llx boot=%llu pcm=%llu cdc=%llu wall=%.3fs rate=%.3fx\n",(unsigned long long)m8rt_get(rt,0),(unsigned long long)m8rt_get(rt,1),(unsigned long long)m8rt_get(rt,2),(unsigned long long)m8rt_get(rt,3),(unsigned long long)m8rt_get(rt,4),(unsigned long long)m8rt_get(rt,9),wall,m8rt_get(rt,4)/176400.0/wall);
    printf("uc=%.3fs callbacks=%.3fs callback_count=%llu\n",m8rt_get(rt,6)/1e9,m8rt_get(rt,7)/1e9,(unsigned long long)m8rt_get(rt,8));
    drain(rt,pcm,1);drain(rt,cdc,0);if(profile) m8rt_dump_profile(rt,profile);
    if(rc) fprintf(stderr,"%s\n",m8rt_error(rt));
    m8rt_destroy(rt);return rc?1:0;
}
