#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from __future__ import absolute_import, division, unicode_literals

import gpg
import hkp4py
import sys

# Copyright (C) 2018 Ben McGinnes <ben@gnupg.org>
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation; either version 2.1 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License and the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU General Public License and the GNU
# Lesser General Public along with this program; if not, see
# <https://www.gnu.org/licenses/>.

print("""
This script imports one or more public keys from the SKS keyservers.
""")

c = gpg.Context()
server = hkp4py.KeyServer("hkps://hkps.pool.sks-keyservers.net")
results = []

if len(sys.argv) > 2:
    pattern = " ".join(sys.argv[1:])
elif len(sys.argv) == 2:
    pattern = sys.argv[1]
else:
    pattern = input("Enter the pattern to search for keys or user IDs: ")

try:
    keys = server.search(pattern)
    if keys is not None:
        print("Found {0} key(s).".format(len(keys)))
    else:
        pass
except Exception as e:
    keys = []
    for logrus in pattern.split():
        if logrus.startswith("0x") is True:
            key = server.search(logrus)
        else:
            key = server.search("0x{0}".format(logrus))
        keys.append(key[0])
    print("Found {0} key(s).".format(len(keys)))

if keys is not None:
    for key in keys:
        import_result = c.key_import(key.key_blob)
        results.append(import_result)

for result in results:
    if result is not None and hasattr(result, "considered") is False:
        print(result)
    elif result is not None and hasattr(result, "considered") is True:
        num_keys = len(result.imports)
        new_revs = result.new_revocations
        new_sigs = result.new_signatures
        new_subs = result.new_sub_keys
        new_uids = result.new_user_ids
        new_scrt = result.secret_imported
        nochange = result.unchanged
        print("""
The total number of keys considered for import was:  {0}

   Number of keys revoked:  {1}
 Number of new signatures:  {2}
    Number of new subkeys:  {3}
   Number of new user IDs:  {4}
Number of new secret keys:  {5}
 Number of unchanged keys:  {6}

The key IDs for all considered keys were:
""".format(num_keys, new_revs, new_sigs, new_subs, new_uids, new_scrt,
           nochange))
        for i in range(num_keys):
            print(result.imports[i].fpr)
        print("")
    else:
        pass
