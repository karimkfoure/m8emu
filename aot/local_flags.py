"""Keep NZCV as private values; pack the word only at state boundaries."""
import re

MACROS = '''#define M8_FLAGS_WORD ((cpu->xpsr & 0x0fffffffu)|(nz_n<<31)|(nz_z<<30)|(nz_c<<29)|(nz_v<<28))
#define M8_FLAGS_GET() do {nz_n=cpu->xpsr>>31;nz_z=(cpu->xpsr>>30)&1u;nz_c=(cpu->xpsr>>29)&1u;nz_v=(cpu->xpsr>>28)&1u;} while(0)
#define M8_FLAGS_SET(value) do {uint32_t flags_value=(value);cpu->xpsr=flags_value;M8_FLAGS_GET();} while(0)
'''
UNDEF = '\n#undef M8_FLAGS_WORD\n#undef M8_FLAGS_GET\n#undef M8_FLAGS_SET\n'
DECLARATIONS = ('uint32_t nz_n=output->xpsr>>31,nz_z=(output->xpsr>>30)&1u,'
                'nz_c=(output->xpsr>>29)&1u,nz_v=(output->xpsr>>28)&1u;\n')


def transform(body):
    body = re.sub(r'cpu->xpsr\s*=\s*([^;]+);', r'M8_FLAGS_SET(\1);', body)
    body = body.replace('cpu->xpsr', 'M8_FLAGS_WORD')
    body = body.replace('(M8_FLAGS_WORD >> 29) & 1u', 'nz_c')
    body = body.replace('m8_condition(M8_FLAGS_WORD,', 'm8_flags_condition(nz_n,nz_z,nz_c,nz_v,')
    for original, replacement in (
        ('m8_nz(cpu,', 'm8_flags_nz(&nz_n,&nz_z,'),
        ('m8_logic_flags(cpu,', 'm8_flags_logic(&nz_n,&nz_z,&nz_c,'),
        ('m8_add_flags(cpu,', 'm8_flags_add(&nz_n,&nz_z,&nz_c,&nz_v,'),
        ('m8_sub_flags(cpu,', 'm8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,'),
    ):
        body = body.replace(original, replacement)
    return body
