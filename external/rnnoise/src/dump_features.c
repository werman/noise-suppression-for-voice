/* Copyright (c) 2024 Jean-Marc Valin
 * Copyright (c) 2017 Mozilla */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "rnnoise.h"
#include "common.h"
#include "denoise.h"
#include "arch.h"
#include "kiss_fft.h"
#include "src/_kiss_fft_guts.h"

int lowpass = FREQ_SIZE;
int band_lp = NB_BANDS;

#define SEQUENCE_LENGTH 2000
#define SEQUENCE_SAMPLES (SEQUENCE_LENGTH*FRAME_SIZE)

#define RIR_FFT_SIZE 65536
#define RIR_MAX_DURATION (RIR_FFT_SIZE/2)
#define FILENAME_MAX_SIZE 1000

struct rir_list {
  int nb_rirs;
  int block_size;
  kiss_fft_state *fft;
  kiss_fft_cpx **rir;
  kiss_fft_cpx **early;
};

kiss_fft_cpx *load_rir(const char *rir_file, kiss_fft_state *fft, int early) {
  kiss_fft_cpx *x, *X;
  float rir[RIR_MAX_DURATION];
  int len;
  int i;
  FILE *f;
  f = fopen(rir_file, "rb");
  if (f==NULL) {
    fprintf(stderr, "cannot open %s: %s\n", rir_file, strerror(errno));
    exit(1);
  }
  x = (kiss_fft_cpx*)calloc(fft->nfft, sizeof(*x));
  X = (kiss_fft_cpx*)calloc(fft->nfft, sizeof(*X));
  len = fread(rir, sizeof(*rir), RIR_MAX_DURATION, f);
  if (early) {
    for (i=0;i<240;i++) {
      rir[480+i] *= (1 - i/240.f);
    }
    RNN_CLEAR(&rir[240+480], RIR_MAX_DURATION-240-480);
  }
  for (i=0;i<len;i++) x[i].r = rir[i];
  rnn_fft_c(fft, x, X);
  free(x);
  fclose(f);
  return X;
}

void load_rir_list(const char *list_file, struct rir_list *rirs) {
  int allocated;
  char rir_filename[FILENAME_MAX_SIZE];
  FILE *f;
  f = fopen(list_file, "rb");
  if (f==NULL) {
    fprintf(stderr, "cannot open %s: %s\n", list_file, strerror(errno));
    exit(1);
  }
  rirs->nb_rirs = 0;
  allocated = 2;
  rirs->fft = rnn_fft_alloc_twiddles(RIR_FFT_SIZE, NULL, NULL, NULL, 0);
  rirs->rir = malloc(allocated*sizeof(rirs->rir[0]));
  rirs->early = malloc(allocated*sizeof(rirs->early[0]));
  while (fgets(rir_filename, FILENAME_MAX_SIZE, f) != NULL) {
    /* Chop trailing newline. */
    rir_filename[strcspn(rir_filename, "\n")] = 0;
    if (rirs->nb_rirs+1 > allocated) {
      allocated *= 2;
      rirs->rir = realloc(rirs->rir, allocated*sizeof(rirs->rir[0]));
      rirs->early = realloc(rirs->early, allocated*sizeof(rirs->early[0]));
    }
    rirs->rir[rirs->nb_rirs] = load_rir(rir_filename, rirs->fft, 0);
    rirs->early[rirs->nb_rirs] = load_rir(rir_filename, rirs->fft, 1);
    rirs->nb_rirs++;
  }
  fclose(f);
}

void rir_filter_sequence(const struct rir_list *rirs, float *audio, int rir_id, int early) {
  int i;
  kiss_fft_cpx x[RIR_FFT_SIZE] = {{0,0}};
  kiss_fft_cpx y[RIR_FFT_SIZE] = {{0,0}};
  kiss_fft_cpx X[RIR_FFT_SIZE] = {{0,0}};
  const kiss_fft_cpx *Y;
  if (early) Y = rirs->early[rir_id];
  else Y = rirs->rir[rir_id];
  i=0;
  while (i<SEQUENCE_SAMPLES) {
    int j;
    RNN_COPY(&x[0], &x[RIR_FFT_SIZE/2], RIR_FFT_SIZE/2);
    for (j=0;j<IMIN(SEQUENCE_SAMPLES-i, RIR_FFT_SIZE/2);j++) x[RIR_FFT_SIZE/2+j].r = audio[i+j];
    for (;j<RIR_FFT_SIZE/2;j++) x[RIR_FFT_SIZE/2+j].r = 0;
    rnn_fft_c(rirs->fft, x, X);
    for (j=0;j<RIR_FFT_SIZE;j++) {
      kiss_fft_cpx tmp;
      C_MUL(tmp, X[j], Y[j]);
      X[j].r = tmp.r*RIR_FFT_SIZE/2;
      X[j].i = tmp.i*RIR_FFT_SIZE/2;
    }
    rnn_ifft_c(rirs->fft, X, y);
    for (j=0;j<IMIN(SEQUENCE_SAMPLES-i, RIR_FFT_SIZE/2);j++) audio[i+j] = y[RIR_FFT_SIZE/2+j].r;
    i += RIR_FFT_SIZE/2;
  }
}

static unsigned rand_lcg(unsigned *seed) {
  *seed = 1664525**seed + 1013904223;
  return *seed;
}

static float uni_rand() {
  return rand()/(double)RAND_MAX-.5;
}

static float randf(float f) {
  return f*rand()/(double)RAND_MAX;
}

static void rand_filt(float *a) {
  if (rand()%3!=0) {
    a[0] = a[1] = 0;
  }
  else if (uni_rand()>0) {
    float r, theta;
    r = rand()/(double)RAND_MAX;
    r = .7*r*r;
    theta = rand()/(double)RAND_MAX;
    theta = M_PI*theta*theta;
    a[0] = -2*r*cos(theta);
    a[1] = r*r;
  } else {
    float r0,r1;
    r0 = 1.4*uni_rand();
    r1 = 1.4*uni_rand();
    a[0] = -r0-r1;
    a[1] = r0*r1;
  }
}

static void rand_resp(float *a, float *b) {
  rand_filt(a);
  rand_filt(b);
}

short speech16[SEQUENCE_LENGTH*FRAME_SIZE];
short noise16[SEQUENCE_LENGTH*FRAME_SIZE];
short fgnoise16[SEQUENCE_LENGTH*FRAME_SIZE];
float x[SEQUENCE_LENGTH*FRAME_SIZE];
float n[SEQUENCE_LENGTH*FRAME_SIZE];
float fn[SEQUENCE_LENGTH*FRAME_SIZE];
float xn[SEQUENCE_LENGTH*FRAME_SIZE];

#define P00 0.99f
#define P01 0.01f
#define P10 0.01f
#define P11 0.99f
#define LOGIT_SCALE 0.5f

static void viterbi_vad(const float *E, int *vad) {
  int i;
  float Enoise, Esig;
  int back[SEQUENCE_LENGTH][2];
  float curr;
  Enoise = Esig = 1e-30;
  for (i=0;i<SEQUENCE_LENGTH;i++) {
    Esig += E[i]*E[i];
  }
  Esig = sqrt(Esig/SEQUENCE_LENGTH);
  for (i=0;i<SEQUENCE_LENGTH;i++) {
    Enoise += 1.f/(1e-8*Esig*Esig + E[i]*E[i]);
  }
  Enoise = 1.f/sqrt(Enoise/SEQUENCE_LENGTH);
  curr = 0.5;
  for (i=0;i<SEQUENCE_LENGTH;i++) {
    float p0, pspeech, pnoise;
    float prior;
    p0 = (log(1e-15+E[i]) - log(Enoise))/(.01 + log(Esig) - log(Enoise));
    p0 = MIN16(.9f, MAX16(.1f, p0));
    p0 = 1.f/(1.f + pow((1.f-p0)/p0, LOGIT_SCALE));
    if (curr*P11 > (1-curr)*P01) {
      back[i][1] = 1;
      prior = curr*P11;
    } else {
      back[i][1] = 0;
      prior = (1-curr)*P01;
    }
    pspeech = prior*p0;

    if ((1-curr)*P00 > curr*P10) {
      back[i][0] = 0;
      prior = (1-curr)*P00;
    } else {
      back[i][0] = 1;
      prior = curr*P10;
    }
    pnoise = prior*(1-p0);
    curr = pspeech / (pspeech + pnoise);
    /*printf("%f ", curr);*/
  }
  vad[SEQUENCE_LENGTH-1] = curr > .5;
  for (i=SEQUENCE_LENGTH-2;i>=0;i--) {
    if (vad[i+1]) {
      vad[i] = back[i+1][1];
    } else {
      vad[i] = back[i+1][0];
    }
  }
  for (i=0;i<SEQUENCE_LENGTH-1;i++) {
    if (vad[i+1]) vad[i] = 1;
  }
  for (i=SEQUENCE_LENGTH-1;i>=1;i--) {
    if (vad[i-1]) vad[i] = 1;
  }
}

static void clear_vad(float *x, int *vad) {
  int i;
  int active = vad[0];
  for (i=0;i<SEQUENCE_LENGTH;i++) {
    if (!active) {
      if (i<SEQUENCE_LENGTH-1 && vad[i+1]) {
        int j;
        for (j=0;j<FRAME_SIZE;j++) x[i*FRAME_SIZE+j] *= j/(float)FRAME_SIZE;
        active = 1;
      } else {
        RNN_CLEAR(&x[i*FRAME_SIZE], FRAME_SIZE);
      }
    } else {
      if (i>=1 && vad[i]==0 && vad[i-1]==0) {
        int j;
        for (j=0;j<FRAME_SIZE;j++) x[i*FRAME_SIZE+j] *= 1.f - j/(float)FRAME_SIZE;
        active = 0;
      }
    }
  }
  /*printf("\n");
  for (i=0;i<SEQUENCE_LENGTH;i++) {
    printf("%d ", vad[i]);
  }
  printf("\n");*/
}

static float weighted_rms(float *x) {
  int i;
  float tmp[SEQUENCE_SAMPLES];
  float weighting_b[2] = {-2.f, 1.f};
  float weighting_a[2] = {-1.89f, .895f};
  float mem[2] = {0};
  float mse = 1e-15f;
  rnn_biquad(tmp, mem, x, weighting_b, weighting_a, SEQUENCE_SAMPLES);
  for (i=0;i<SEQUENCE_SAMPLES;i++) mse += tmp[i]*tmp[i];
  return 0.9506*sqrt(mse/SEQUENCE_SAMPLES);
}

int main(int argc, char **argv) {
  int i, j;
  int count=0;
  static const float a_hp[2] = {-1.99599, 0.99600};
  static const float b_hp[2] = {-2, 1};
  float a_noise[2] = {0};
  float b_noise[2] = {0};
  float a_fgnoise[2] = {0};
  float b_fgnoise[2] = {0};
  float a_sig[2] = {0};
  float b_sig[2] = {0};
  float speech_gain = 1, noise_gain = 1, fgnoise_gain = 1;
  FILE *f1, *f2, *f3, *fout;
  long speech_length, noise_length, fgnoise_length;
  int maxCount;
  unsigned seed;
  DenoiseState *st;
  DenoiseState *noisy;
  char *argv0;
  char *rir_filename = NULL;
  struct rir_list rirs;
  seed = getpid();
  srand(seed);
  st = rnnoise_create(NULL);
  noisy = rnnoise_create(NULL);
  argv0 = argv[0];
  while (argc>6) {
    if (strcmp(argv[1], "-rir_list")==0) {
      rir_filename = argv[2];
      argv+=2;
      argc-=2;
    }
  }
  if (argc!=6) {
    fprintf(stderr, "usage: %s [-rir_list list] <speech> <noise> <fg_noise> <output> <count>\n", argv0);
    return 1;
  }
  f1 = fopen(argv[1], "rb");
  f2 = fopen(argv[2], "rb");
  f3 = fopen(argv[3], "rb");
  fout = fopen(argv[4], "wb");

  fseek(f1, 0, SEEK_END);
  speech_length = ftell(f1);
  fseek(f1, 0, SEEK_SET);
  
  fseek(f2, 0, SEEK_END);
  noise_length = ftell(f2);
  fseek(f2, 0, SEEK_SET);

  fseek(f3, 0, SEEK_END);
  fgnoise_length = ftell(f3);
  fseek(f3, 0, SEEK_SET);

  maxCount = atoi(argv[5]);
  if (rir_filename) load_rir_list(rir_filename, &rirs);
  for (count=0;count<maxCount;count++) {
    int rir_id;
    int vad[SEQUENCE_LENGTH];
    long speech_pos, noise_pos, fgnoise_pos;
    int start_pos=0;
    float E[SEQUENCE_LENGTH] = {0};
    float mem[2]={0};
    int frame;
    int silence;
    kiss_fft_cpx X[FREQ_SIZE], Y[FREQ_SIZE], P[WINDOW_SIZE];
    float Ex[NB_BANDS], Ey[NB_BANDS], Ep[NB_BANDS];
    float Exp[NB_BANDS];
    float features[NB_FEATURES];
    float g[NB_BANDS];
    float speech_rms, noise_rms, fgnoise_rms;
    if ((count%1000)==0) fprintf(stderr, "%d\r", count);
    speech_pos = (rand_lcg(&seed)*2.3283e-10)*speech_length;
    noise_pos = (rand_lcg(&seed)*2.3283e-10)*noise_length;
    fgnoise_pos = (rand_lcg(&seed)*2.3283e-10)*fgnoise_length;
    if (speech_pos > speech_length-(long)sizeof(speech16)) speech_pos = speech_length-sizeof(speech16);
    if (noise_pos > noise_length-(long)sizeof(noise16)) noise_pos = noise_length-sizeof(noise16);
    if (fgnoise_pos > fgnoise_length-(long)sizeof(fgnoise16)) fgnoise_pos = fgnoise_length-sizeof(fgnoise16);
    speech_pos -= speech_pos&1;
    noise_pos -= noise_pos&1;
    fgnoise_pos -= fgnoise_pos&1;
    fseek(f1, speech_pos, SEEK_SET);
    fseek(f2, noise_pos, SEEK_SET);
    fseek(f3, fgnoise_pos, SEEK_SET);
    fread(speech16, sizeof(speech16), 1, f1);
    fread(noise16, sizeof(noise16), 1, f2);
    fread(fgnoise16, sizeof(fgnoise16), 1, f3);
    if (rand()%4) start_pos = 0;
    else start_pos = -(int)(1000*log(rand()/(float)RAND_MAX));
    start_pos = IMIN(start_pos, SEQUENCE_LENGTH*FRAME_SIZE);

    speech_gain = pow(10., (-45+randf(45.f)+randf(10.f))/20.);
    noise_gain = pow(10., (-30+randf(40.f)+randf(15.f))/20.);
    fgnoise_gain = pow(10., (-30+randf(40.f)+randf(15.f))/20.);
    if (rand()%8==0) noise_gain = 0;
    if (rand()%8!=0) fgnoise_gain = 0;
    if (rand()%12==0) {
      noise_gain *= 0.03;
      fgnoise_gain *= 0.03;
    }
    noise_gain *= speech_gain;
    fgnoise_gain *= speech_gain;
    rand_resp(a_noise, b_noise);
    rand_resp(a_fgnoise, b_fgnoise);
    rand_resp(a_sig, b_sig);
    lowpass = FREQ_SIZE * 3000./24000. * pow(50., rand()/(double)RAND_MAX);
    for (i=0;i<NB_BANDS;i++) {
      if (eband20ms[i] > lowpass) {
        band_lp = i;
        break;
      }
    }

    for (frame=0;frame<SEQUENCE_LENGTH;frame++) {
      E[frame] = 0;
      for(j=0;j<FRAME_SIZE;j++) {
        float s = speech16[frame*FRAME_SIZE+j];
        E[frame] += s*s;
        x[frame*FRAME_SIZE+j] = speech16[frame*FRAME_SIZE+j];
        n[frame*FRAME_SIZE+j] = noise16[frame*FRAME_SIZE+j];
        fn[frame*FRAME_SIZE+j] = fgnoise16[frame*FRAME_SIZE+j];
      }
    }
    viterbi_vad(E, vad);

    RNN_CLEAR(mem, 2);
    rnn_biquad(x, mem, x, b_hp, a_hp, SEQUENCE_LENGTH*FRAME_SIZE);
    RNN_CLEAR(mem, 2);
    rnn_biquad(x, mem, x, b_sig, a_sig, SEQUENCE_LENGTH*FRAME_SIZE);
    RNN_CLEAR(mem, 2);
    rnn_biquad(n, mem, n, b_hp, a_hp, SEQUENCE_LENGTH*FRAME_SIZE);
    RNN_CLEAR(mem, 2);
    rnn_biquad(n, mem, n, b_noise, a_noise, SEQUENCE_LENGTH*FRAME_SIZE);
    RNN_CLEAR(mem, 2);
    rnn_biquad(fn, mem, fn, b_hp, a_hp, SEQUENCE_LENGTH*FRAME_SIZE);
    RNN_CLEAR(mem, 2);
    rnn_biquad(fn, mem, fn, b_fgnoise, a_fgnoise, SEQUENCE_LENGTH*FRAME_SIZE);

    speech_rms = weighted_rms(x);
    noise_rms = weighted_rms(n);
    fgnoise_rms = weighted_rms(fn);

    RNN_CLEAR(vad, start_pos/FRAME_SIZE);
    clear_vad(x, vad);

    speech_gain *= 3000.f/(1+speech_rms);
    noise_gain *= 3000.f/(1+noise_rms);
    fgnoise_gain *= 3000.f/(1+fgnoise_rms);
    for (j=0;j<SEQUENCE_SAMPLES;j++) {
      x[j] *= speech_gain;
      n[j] *= noise_gain;
      fn[j] *= fgnoise_gain;
      xn[j] = x[j] + n[j] + fn[j];
    }
    if (rir_filename && rand()%2==0) {
      rir_id = rand()%rirs.nb_rirs;
      rir_filter_sequence(&rirs, x, rir_id, 1);
      rir_filter_sequence(&rirs, xn, rir_id, 0);
    }
    if (rand()%4==0) {
      /* Apply input clipping to 0 dBFS (don't clip target). */
      for (j=0;j<SEQUENCE_SAMPLES;j++) {
        xn[j] = MIN16(32767.f, MAX16(-32767.f, xn[j]));
      }
    }
    if (rand()%2==0) {
      /* Apply 16-bit quantization. */
      for (j=0;j<SEQUENCE_SAMPLES;j++) {
        xn[j] = floor(.5f + xn[j]);
      }
    }
    for (frame=0;frame<SEQUENCE_LENGTH;frame++) {
      float vad_target;
      rnn_frame_analysis(st, Y, Ey, &x[frame*FRAME_SIZE]);
      silence = rnn_compute_frame_features(noisy, X, P, Ex, Ep, Exp, features, &xn[frame*FRAME_SIZE]);
      /*rnn_pitch_filter(X, P, Ex, Ep, Exp, g);*/
      vad_target = vad[frame];
      for (i=0;i<NB_BANDS;i++) {
        g[i] = sqrt((Ey[i]+1e-3)/(Ex[i]+1e-3));
        if (g[i] > 1) g[i] = 1;
        if (silence || i > band_lp) g[i] = -1;
        if (Ey[i] < 5e-2 && Ex[i] < 5e-2) g[i] = -1;
        if (vad_target==0 && noise_gain==0 && fgnoise_gain==0) g[i] = -1;
      }
#if 0
      {
        short tmp[FRAME_SIZE];
        for (j=0;j<FRAME_SIZE;j++) tmp[j] = MIN16(32767, MAX16(-32767, xn[frame*FRAME_SIZE+j]));
        fwrite(tmp, FRAME_SIZE, 2, fout);
      }
#endif
#if 1
      fwrite(features, sizeof(float), NB_FEATURES, fout);
      fwrite(g, sizeof(float), NB_BANDS, fout);
      fwrite(&vad_target, sizeof(float), 1, fout);
#endif
    }
  }

  fclose(f1);
  fclose(f2);
  fclose(f3);
  fclose(fout);
  return 0;
}
