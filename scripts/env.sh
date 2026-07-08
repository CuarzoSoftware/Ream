#!/bin/bash

# Exported for the Doxyfile (PROJECT_NUMBER) and packaging scripts.

export REAM_DEB_PACKAGE_NAME=libcz-ream-dev
export REAM_RPM_PACKAGE_NAME=libcz-ream-devel
export REAM_DEB_PACKAGE_ARCH=amd64
export REAM_RPM_PACKAGE_ARCH=x86_64
export REAM_VERSION=`cat ./../VERSION`
export REAM_BUILD=`cat ./../BUILD`
