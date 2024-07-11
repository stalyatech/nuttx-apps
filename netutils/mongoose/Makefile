############################################################################
# apps/netutils/mongoose/Makefile
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

include $(APPDIR)/Make.defs

MONGOOSE_URL ?= "https://github.com/cesanta/mongoose/archive/refs/tags"

MONGOOSE_VERSION = $(patsubst "%",%,$(strip $(CONFIG_MONGOOSE_VERSION)))
MONGOOSE_ZIP = $(CONFIG_MONGOOSE_VERSION).zip

MONGOOSE_UNPACKNAME = mongoose
UNPACK ?= unzip -q -o

$(MONGOOSE_ZIP):
	@echo "Downloading: $(MONGOOSE_URL)/$(MONGOOSE_ZIP)"
	$(Q) curl -O -L $(MONGOOSE_URL)/$(MONGOOSE_ZIP)

$(MONGOOSE_UNPACKNAME): $(MONGOOSE_ZIP)
	@echo "Unpacking: $(MONGOOSE_ZIP) -> $(MONGOOSE_UNPACKNAME)"
	$(Q) $(UNPACK) $(MONGOOSE_ZIP)
	$(Q) mv mongoose-$(MONGOOSE_VERSION) $(MONGOOSE_UNPACKNAME)
	$(Q) touch $(MONGOOSE_UNPACKNAME)

# Download and unpack tarball if no git repo found
ifeq ($(wildcard $(MONGOOSE_UNPACKNAME)),)
context:: $(MONGOOSE_UNPACKNAME)

distclean::
	$(call DELDIR, $(MONGOOSE_UNPACKNAME))
	$(call DELFILE, $(MONGOOSE_ZIP))
endif

CFLAGS += -DMG_ARCH=MG_ARCH_UNIX -DMG_ENABLE_SSI=1 -DMG_IO_SIZE=2048 -DMG_TLS=MG_TLS_BUILTIN -DMG_PATH_MAX=256 -DMG_SSI_BUFSIZ=256

CSRCS += mongoose/mongoose.c

include $(APPDIR)/Application.mk
