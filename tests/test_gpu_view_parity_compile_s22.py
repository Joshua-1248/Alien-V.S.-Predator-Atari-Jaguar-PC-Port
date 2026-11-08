#!/usr/bin/env python3
"""Compile the actual ROM-gated GPU parity entrypoint with synthetic metadata.
No original 3D GPU instructions or resource bytes are generated/distributed."""
import os
import subprocess
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='re24-s22-parity-') as tmp:
    tmp=Path(tmp)
    fake=tmp/'synthetic_pipeline.c'
    fake.write_text('''
static uint32_t dummy_stage(AvpGpuRecompCtx *g,uint32_t pc){(void)g;return pc;}
static const struct {uint32_t entry,table,src,longs;
  uint32_t (*fn)(AvpGpuRecompCtx*,uint32_t);} avp_gpu_stages[] = {
    {0xF03000u,0x7F64u,0x1000u,1u,dummy_stage}
};
#define AVP_GPU_STAGE_COUNT 1u
static const uint64_t avp_gpu_resident_hash=UINT64_C(1);
static const uint64_t avp_gpu_overlay_hashes[]={UINT64_C(1)};
''')
    cc=os.environ.get('CC','gcc')
    cmd=[cc,'-std=c99','-O2','-Wall','-Wextra','-Werror',
         '-I'+str(root/'include'),'-DAVP_GPU_NATIVE_EXEC',
         '-DPIPELINE_SOURCE="'+str(fake)+'"','-c',
         str(root/'tools/jagref/gpu_pipeline_check.c'),'-o',str(tmp/'check.o')]
    subprocess.run(cmd,check=True)
print('RE24 S22 ROM-gated GPU view parity entrypoint strict compile PASS ('+cc+')')
