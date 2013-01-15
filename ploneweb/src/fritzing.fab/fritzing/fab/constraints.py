# -*- coding:utf-8 -*-
import re
import zipfile
from cStringIO import StringIO

from z3c.form import validator
from zope.interface import Invalid

from fritzing.fab import getboardsize
from fritzing.fab import _


class SketchFileValidator(validator.SimpleFieldValidator):
    
    def validate(self, sketchFile):
        super(SketchFileValidator, self).validate(sketchFile)
        
        fzzName = sketchFile.filename
        fzzNameLower = fzzName.lower()
        
        if not (fzzNameLower.endswith('.fzz')):
            raise Invalid(
                _(u"We can only produce from shareable Fritzing sketch files (.fzz)"))
        
        filenameMatch = re.compile(
            r"^[a-zA-Z0-9 _\.\(\)\-\+]+$").match
        if not filenameMatch(fzzName):
            raise Invalid(
                _(u"Please change the sketch name to not contain any special characters such as */?$§&"))

        # use StringIO to make the blob to look like a file object:
        fzzData = StringIO(sketchFile.data)
        
        zf = None
        try:
            zf = zipfile.ZipFile(fzzData) 
        except:
            raise Invalid(
                _(u"Hmmm, '%s' doesn't seem to be a valid .fzz file. Sorry, we only support those at this time." % fzzName))
        
        pairs = getboardsize.fromZipFile(zf, fzzName)
        
        if not (len(pairs) >= 2):
            raise Invalid(
                _(u"No boards found in '%s'." % fzzName))
        
        if not (len(pairs)%2 == 0):
            # uneven number of lengths
            raise Invalid(
                _(u"Invalid board sizes in '%s'." % fzzName))

        return True


eMailMatch = re.compile(
    r"[a-zA-Z0-9._%-]+@([a-zA-Z0-9-]+\.)*[a-zA-Z]{2,4}").match

def checkEMail(eMailAddress):
    """Check if the e-mail address looks valid
    """
    if not (eMailMatch(eMailAddress)):
        raise Invalid(_(u"Invalid e-mail address"))
    return True

