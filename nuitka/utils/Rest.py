#     Copyright 2021, Kay Hayen, mailto:kay.hayen@gmail.com
#
#     Part of "Nuitka", an optimizing Python compiler that is compatible and
#     integrates with CPython, but also works on its own.
#
#     Licensed under the Apache License, Version 2.0 (the "License");
#     you may not use this file except in compliance with the License.
#     You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#     Unless required by applicable law or agreed to in writing, software
#     distributed under the License is distributed on an "AS IS" BASIS,
#     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#     See the License for the specific language governing permissions and
#     limitations under the License.
#
""" Create PDF documentations.

This ought to be in "nuitka.tools.release" but the packaging might happen
on release package, as in case of Debian, therefore it's here.

"""

import os
import tempfile

from .Execution import check_call
from .FileOperations import getFileContents, putTextFileContents


def createPDF(document):
    args = []

    with tempfile.NamedTemporaryFile(delete=False) as style_file:
        style_filename = style_file.name
        style_file.write(
            """
"pageSetup" : {
   "firstTemplate": "coverPage"
}

"styles" : [
       [ "title" , {
           "fontName": "NanumGothic-Bold",
           "fontSize": 40
       } ],
       [ "heading1" , {
           "fontName": "NanumGothic-Bold"
       } ],
       [ "heading2" , {
           "fontName": "NanumGothic"
       } ]
]
"""
        )

    if document != "Changelog.rst":
        args.append("-s")
        args.append(style_filename)

        args.append('--header="###Title### - ###Section###"')
        args.append('--footer="###Title### - page ###Page### - ###Section###"')

    # Workaround for rst2pdf not support ..code:: without language.
    old_contents = getFileContents(document)
    new_contents = old_contents.replace(".. code::\n", "::\n")

    # Add page counter reset right after TOC for PDF.
    new_contents = new_contents.replace(
        ".. contents::",
        """.. contents::

.. raw:: pdf

    PageBreak oneColumn
    SetPageCounter 1

""",
    )

    try:
        if new_contents != old_contents:
            putTextFileContents(filename=document, contents=new_contents)

        check_call(["rst2pdf"] + args + [document])
    finally:
        if new_contents != old_contents:
            putTextFileContents(filename=document, contents=old_contents)

    os.unlink(style_filename)