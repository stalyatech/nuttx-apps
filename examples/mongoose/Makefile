############################################################################
# apps/examples/mongoose/Makefile
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

# Compiler directives

CFLAGS += -DMG_ARCH=MG_ARCH_UNIX -DMG_ENABLE_SSI=1 -DMG_IO_SIZE=2048 -DMG_TLS=MG_TLS_BUILTIN -DMG_PATH_MAX=256 -DMG_SSI_BUFSIZ=256

# Source files

ifneq ($(CONFIG_EXAMPLES_MONGOOSE_HTTPD),)
CSRCS    += httpd/net.c
MAINSRC  += httpd/httpd.c
PROGNAME += mg_httpd
endif

ifneq ($(CONFIG_EXAMPLES_MONGOOSE_WEBSOCKD),)
MAINSRC  += websockd/websockd.c
PROGNAME += mg_wsockd
endif

ifneq ($(CONFIG_EXAMPLES_MONGOOSE_SMARTANT),)

# uORB messaging
ifneq ($(CONFIG_EXAMPLES_MONGOOSE_SMARTANT_UORB),)
MAINSRC  += smart/httpd_uorb.c
endif

# FIFO messaging
ifneq ($(CONFIG_EXAMPLES_MONGOOSE_SMARTANT_FIFO),)
MAINSRC  += smart/httpd_fifo.c
endif

PROGNAME += mg_smart
endif

PRIORITY  = $(CONFIG_EXAMPLES_MONGOOSE_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_MONGOOSE_STACKSIZE)
MODULE    = $(CONFIG_EXAMPLES_MONGOOSE)

include $(APPDIR)/Application.mk
