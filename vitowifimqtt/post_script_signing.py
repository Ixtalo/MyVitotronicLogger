import os
from signing import sign_and_write



Import("env", "projenv")
# access to global construction environment
print(env)
# access to project construction environment
print(projenv)
# Dump construction environments (for debug purpose)
#print(env.Dump())
#print(projenv.Dump())


## custom options
## https://docs.platformio.org/en/latest/projectconf/advanced_scripting.html#custom-options-in-platformio-ini
try:
    import configparser
except ImportError:
    import ConfigParser as configparser
config = configparser.ConfigParser()
config.read("platformio.ini")
privkey_filepath = config.get("env:d1_mini", "signing_private_key")
privkey_filepath = os.path.expandvars(privkey_filepath)




def post_build_sign(source, target, env):
    binary_filepath = os.path.abspath(os.path.join(env['PROJECT_BUILD_DIR'], env['PIOENV'], env['PROGNAME'] + '.bin'))
    binary_signed_filepath = binary_filepath + '.signed'
    with open(binary_filepath, 'rb') as fin:
        bin = fin.read()
        sign_and_write(bin, privkey_filepath, binary_signed_filepath)

env.AddPostAction("buildprog", post_build_sign)


