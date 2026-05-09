"""
/* Copyright (c) 2024 Jean-Marc Valin */
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
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
"""

import torch
from torch import nn
import torch.nn.functional as F
import sys
import os

sys.path.append(os.path.join(os.path.split(__file__)[0], '..'))
from sparsification import GRUSparsifier

sparsify_start     = 6000
sparsify_stop      = 20000
sparsify_interval  = 100
sparsify_exponent  = 3

sparse_params1 = {
                'W_hr' : (0.3, [8, 4], True),
                'W_hz' : (0.2, [8, 4], True),
                'W_hn' : (0.5, [8, 4], True),
                'W_ir' : (0.3, [8, 4], False),
                'W_iz' : (0.2, [8, 4], False),
                'W_in' : (0.5, [8, 4], False)
                }

def init_weights(module):
    if isinstance(module, nn.GRU):
        for p in module.named_parameters():
            if p[0].startswith('weight_hh_'):
                nn.init.orthogonal_(p[1])

class RNNoise(nn.Module):
    def __init__(self, input_dim=65, output_dim=32, cond_size=128, gru_size=256):
        super(RNNoise, self).__init__()

        self.input_dim = input_dim
        self.output_dim = output_dim
        self.cond_size = cond_size
        self.gru_size = gru_size
        self.conv1 = nn.Conv1d(input_dim, cond_size, kernel_size=3, padding='valid')
        self.conv2 = nn.Conv1d(cond_size, gru_size, kernel_size=3, padding='valid')
        self.gru1 = nn.GRU(self.gru_size, self.gru_size, batch_first=True)
        self.gru2 = nn.GRU(self.gru_size, self.gru_size, batch_first=True)
        self.gru3 = nn.GRU(self.gru_size, self.gru_size, batch_first=True)
        self.dense_out = nn.Linear(4*self.gru_size, self.output_dim)
        self.vad_dense = nn.Linear(4*self.gru_size, 1)
        nb_params = sum(p.numel() for p in self.parameters())
        print(f"model: {nb_params} weights")
        self.apply(init_weights)
        self.sparsifier = []
        self.sparsifier.append(GRUSparsifier([(self.gru1, sparse_params1)], sparsify_start, sparsify_stop, sparsify_interval, sparsify_exponent))
        self.sparsifier.append(GRUSparsifier([(self.gru2, sparse_params1)], sparsify_start, sparsify_stop, sparsify_interval, sparsify_exponent))
        self.sparsifier.append(GRUSparsifier([(self.gru3, sparse_params1)], sparsify_start, sparsify_stop, sparsify_interval, sparsify_exponent))


    def sparsify(self):
        for sparsifier in self.sparsifier:
            sparsifier.step()

    def forward(self, features, states=None):
        #print(states)
        device = features.device
        batch_size = features.size(0)
        if states is None:
            gru1_state = torch.zeros((1, batch_size, self.gru_size), device=device)
            gru2_state = torch.zeros((1, batch_size, self.gru_size), device=device)
            gru3_state = torch.zeros((1, batch_size, self.gru_size), device=device)
        else:
            gru1_state = states[0]
            gru2_state = states[1]
            gru3_state = states[2]
        tmp = features.permute(0, 2, 1)
        tmp = torch.tanh(self.conv1(tmp))
        tmp = torch.tanh(self.conv2(tmp))
        tmp = tmp.permute(0, 2, 1)

        gru1_out, gru1_state = self.gru1(tmp, gru1_state)
        gru2_out, gru2_state = self.gru2(gru1_out, gru2_state)
        gru3_out, gru3_state = self.gru3(gru2_out, gru3_state)
        out_cat = torch.cat([tmp, gru1_out, gru2_out, gru3_out], dim=-1)
        gain = torch.sigmoid(self.dense_out(out_cat))
        vad = torch.sigmoid(self.vad_dense(out_cat))
        return gain, vad, [gru1_state, gru2_state, gru3_state]
