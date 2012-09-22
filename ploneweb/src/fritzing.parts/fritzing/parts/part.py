from five import grok
from zope import schema
from plone.directives import form

from zope.interface import Invalid

from zope.schema import Text, ASCII, ASCIILine
from plone.app.textfield import RichText
from plone.namedfile.field import NamedBlobFile
from z3c.relationfield.schema import RelationList, RelationChoice
from plone.formwidget.contenttree import ObjPathSourceBinder

from zope.lifecycleevent.interfaces import IObjectCreatedEvent, IObjectModifiedEvent

import zipfile, xml.dom.minidom, xml.dom
from cStringIO import StringIO

from fritzing.parts import FritzingPartsMessageFactory as _

def fritzingFilenameIsValid(fileField):
    """Constraint function to check for valid file name/type.
    """
    if fileField:
        fzpzName = fileField.filename
        fzpzNameLower = fzpzName.lower()
        if not fzpzNameLower.endswith(".fzpz"):
            raise Invalid(
                _(u"Only accept Fritzing .fzpz files")
            )
#        extractSVG(fileField)
    return True


class IPart(form.Schema):
    """A Fritzing Part
    """
    title = schema.ASCIILine(
        title=_(u"Name"),
        description=_(u"The name of the part"),
    )
    description = Text(
        title=_(u"Teaser"),
        description=_(u"Give a one-line description of this part"),
    )
    body = RichText(
        title=_(u"Description"),
        description=_(u"Describe this part as detailed and structured as possible"),
        required=False,
    )
    fritzingFile = NamedBlobFile(
        title=_(u"Fritzing Part File"),
        description=_(u"The .fzpz file of the part"),
        constraint=fritzingFilenameIsValid,
    )
    form.omitted('iconView')
    iconView = ASCII(
        title=_(u"Breaboard View"),
        description=_(u"The SVG graphics for the breadboard view"),
    )
    form.omitted('breadboardView')
    breadboardView = ASCII(
        title=_(u"Breaboard View"),
        description=_(u"The SVG graphics for the breadboard view"),
    )
    form.omitted('schematicView')
    schematicView = ASCII(
        title=_(u"Schematic View"),
        description=_(u"The SVG graphics for the schematic view"),
    )
    form.omitted('pcbView')
    pcbView = ASCII(
        title=_(u"PCB View"),
        description=_(u"The SVG graphics for the pcb view"),
    )
    form.omitted('fritzingVersion')
    fritzingVersion = ASCIILine(
        title=_(u"Fritzing Version"),
        description=_(u"The Fritzing version that this part was created with."),
    )
    form.omitted('moduleId')
    moduleId = ASCIILine(
        title=_(u"Module ID"),
        description=_(u"The unique identifier of this part."),
    )

class View(grok.View):
    """Default view (called "@@view"") for a part.
    
    The associated template is found in part_templates/view.pt.
    """
    
    grok.context(IPart)
    grok.require('zope2.View')
    grok.name('view')
    
    def update(self):
        """Called before rendering the template for this view
        """        


@grok.subscribe(IPart, IObjectCreatedEvent)
@grok.subscribe(IPart, IObjectModifiedEvent)
def extractFZPZ(part, event):
    fileData = StringIO(part.fritzingFile.data)
    zf = None
    try:
        zf = zipfile.ZipFile(fileData) 
    except:
        raise Invalid(
            _(u"This fzpz file seems corrupted.")
        )
        
    iconSVG = None
    breadboardSVG = None
    schematicSVG = None
    pcbSVG = None
    fzpXML = None
    for i, name in enumerate(zf.namelist()):   
        if name.endswith('.svg'):
            if name.startswith('svg.icon.'):
                iconSVG = zf.read(name)
            if name.startswith('svg.breadboard.'):
                breadboardSVG = zf.read(name)
            if name.startswith('svg.schematic.'):
                schematicSVG = zf.read(name)
            if name.startswith('svg.pcb.'):
                pcbSVG = zf.read(name)
        if name.endswith('.fzp'):
            fzpXML = zf.read(name)
    if not iconSVG:
        raise Invalid(
            _(u"No SVG icon graphics found in fzpz.")
        )
    if not breadboardSVG:
        raise Invalid(
            _(u"No SVG breadboard graphics found in fzpz.")
        )
    if not schematicSVG:
        raise Invalid(
            _(u"No SVG schematic graphics found in fzpz.")
        )
    if not pcbSVG:
        raise Invalid(
            _(u"No SVG pcb graphics found in fzpz.")
        )
    if not fzpXML:
        raise Invalid(
            _(u"No fzp part description found in fzpz.")
        )
    part.iconView = iconSVG
    part.breadboardView = breadboardSVG
    part.schematicView = schematicSVG
    part.pcbView = pcbSVG
    
    # Parse FZP
    dom = None
    try:
        dom = xml.dom.minidom.parseString(fzpXML)
    except:
        raise Invalid(
            _(u"The fzp is not a valid XML file.")
        )
    if dom == None:
        raise Invalid(
            _(u"The fzp is not a valid XML file (2).")
        )
    root = dom.documentElement
    if root.tagName != 'module':
        raise Invalid(
            _(u"Missing required 'module' root element in fzp.")
        )

    part.fritzingVersion = root.getAttribute('fritzingVersion')
    part.moduleId = root.getAttribute('moduleId')
