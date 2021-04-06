##############################################################################
# Copyright (c) 2019       The University of Tennessee and the University
#                          of Tennessee Research Foundation.  All rights
#                          reserved.
#
# $COPYRIGHT$
#
##############################################################################
from spack import *

class Dplasma(CMakePackage):
    """DPLASMA: The distributed, accelerator enabled, dense linear algebra library"""

    homepage = "https://bitbucket.org/icldistcomp/dplasma"
    url      = "https://bitbucket.org/icldistcomp/dplasma/get/v2.0.0.tar.bz2"
    list_url = "https://bitbucket.org/icldistcomp/dplasma/downloads/?tab=tags"
    git      = "https://bitbucket.org/icldistcomp/dplasma.git"

    version('master', branch='master')
    version('2.0.0', '')

    variant('install_tests', default=False, description='Install tests')

    depends_on('cmake@3.16.0:', type='build')
    depends_on('parsec')
    depends_on('lapack')

    def cmake_args(self):
        args = [
            self.define_from_variant('DPLASMA_INSTALL_TESTS', 'install_tests'),
        ]
        args.extend(self.spec['lapack'].cmake_args)
        return args

    # Inherit all these features from PaRSEC
    #variant('cuda', default=True, description='Use CUDA for GPU acceleration')
    #variant('profile', default=False, description='Generate profiling data')
    #variant('debug', default=False, description='Debug version (incurs performance overhead!)')


