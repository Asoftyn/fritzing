import string
import random
import csv
from DateTime import DateTime
from StringIO import StringIO
import zipfile
try:
    import zlib
    compression = zipfile.ZIP_DEFLATED
except:
    compression = zipfile.ZIP_STORED

from five import grok
from Acquisition import aq_inner

from plone.directives import dexterity

from plone.memoize.instance import memoize
from Products.CMFCore.utils import getToolByName

from fritzing.fab.interfaces import IFabOrders, IFabOrder
from fritzing.fab import _


class Index(grok.View):
    grok.context(IFabOrders)
    message = None
    
    label = _(u"Fritzing Fab")
    description = _(u"There's nothing better than turning a concept into product reality.")
    
    # make tools availible to the template as view.toolname()
    from fritzing.fab.tools \
        import encodeFilename, getStateId, getStateTitle, isStateId, canDelete
    
    def update(self):
        self.member = self.context.portal_membership.getAuthenticatedMember()
        self.isManager = self.member.has_role('Manager')
        self.isOwner = self.member.has_role('Owner')
        self.hasOrders = len(self.listOrders()) > 0
        if not (self.isManager):
            self.request.set('disable_border', 1)

    @memoize
    def listOrders(self):
        """list orders visible for this user
        """
        catalog = getToolByName(self.context, 'portal_catalog')

        query = {
            'object_provides': IFabOrder.__identifier__,
            'path': dict(query='/'.join(self.context.getPhysicalPath()),
                         depth=1),
            'sort_on': 'Date',
            'Creator': self.member.id, }

        return [orderBrain.getObject() for orderBrain in catalog(query)]

    @memoize
    def listOrdersManager(self):
        """list orders visible for this user
        """
        catalog = getToolByName(self.context, 'portal_catalog')

        query = {
            'object_provides': IFabOrder.__identifier__,
            'path': dict(query='/'.join(self.context.getPhysicalPath()),
                         depth=1),
            'sort_on': 'Date',
            'review_state': 'in_process', }

        return [orderBrain.getObject() for orderBrain in catalog(query)]



class CurrentOrders(grok.View):
    """Overview of current orders
    """
    grok.name('currentorders')
    grok.require('cmf.ModifyPortalContent')
    grok.context(IFabOrders)
    
    # make tools availible to the template as view.toolname()
    from fritzing.fab.tools \
        import getCurrentOrders, encodeFilename, getStateId, getStateTitle, isStateId

    def update(self):
        pass
    

class CurrentOrdersCSV(grok.View):
    """All current orders as CSV table
    """
    grok.name('currentorders-csv')
    grok.require('cmf.ModifyPortalContent')
    grok.context(IFabOrders)
    
    # make tools availible to the template as view.toolname()
    from fritzing.fab.tools \
        import getCurrentOrders, encodeFilename

    def update(self):
        pass
    
    def render(self):
        """All current orders as CSV table
        """
        out = StringIO()
        context = aq_inner(self.context)
        writer = csv.writer(out)
        
        # Header
        writer.writerow(('date', 'order-id', 'filename', 'count', 'optional', 'boards', 'size', 'price', 'paid', 'checked', 'sent', 'invoiced', 'bloggable', 'name', 'e-mail', 'zone'))
        # Content
        for brain in self.getCurrentOrders(context):
            order = brain.getObject()
            for sketch in order.listFolderContents():
                writer.writerow((
                    DateTime(order.Date()).strftime('%y-%m-%d %H:%M:%S'), 
                    order.id,
                    order.id + "_" + self.encodeFilename(sketch.orderItem.filename),
                    sketch.copies,
                    "",
                    len(sketch.boards),
                    '%.2f' % sketch.area,
                    '%.2f' % (sketch.copies * sketch.area * order.pricePerSquareCm + 4.0),
                    "",
                    "",
                    "",
                    "",
                    "",
                    order.getOwner(),
                    order.email,
                    order.shipTo
                ))
        
        # Prepare response
        filename = "fab-orders-%s.csv" % context.Date()
        self.request.response.setHeader('Content-Type', 'text/csv')
        self.request.response.setHeader('Content-Disposition', 'attachment; filename="%s"' % filename)
        
        return out.getvalue()


class CurrentOrdersZIP(grok.View):
    """All current orders as ZIP file
    """
    grok.name('currentorders-zip')
    grok.require('cmf.ModifyPortalContent')
    grok.context(IFabOrders)
    
    # make tools availible to the template as view.toolname()
    from fritzing.fab.tools \
        import getCurrentOrders, encodeFilename

    def update(self):
        pass
    
    def render(self):
        """All current orders as ZIP file
        """
        out = StringIO()
        context = aq_inner(self.context)
        zipfilename = "fab-orders-%s.zip" % context.Date()
        
        zf = zipfile.ZipFile(
            out, 
            mode='w',
            compression = compression)
        try:
            for brain in self.getCurrentOrders(context):
                order = brain.getObject()
                for sketch in order.listFolderContents():
                    zf.writestr(
                        order.id + "_" + self.encodeFilename(sketch.orderItem.filename),
                        sketch.orderItem.data
                    )
        finally:
            zf.close()

        # Prepare response
        self.request.response.setHeader('Content-Type', 'application/octet-stream')
        self.request.response.setHeader('Content-Disposition', 'attachment; filename="%s"' % zipfilename)
        
        out.seek(0)
        content = out.read()
        out.close()
        return content


class PayPalIpn(grok.View):
    """Payment Confirmation
    """
    grok.name('paypal_ipn')
    grok.require('zope2.View')
    grok.context(IFabOrders)
    
    def update(self):
        pass
    
    def render(self):
        pass


class AddForm(dexterity.AddForm):
    """creates a new faborder transparently
    """
    grok.name('faborder')
    grok.require('cmf.AddPortalContent')
    grok.context(IFabOrders)
    
    schema = IFabOrder
    
    label = _(u"New Fab Order")
    description = _(u"Creates a new order for the Fritzing Fab Service")
    
    def create(self, data):
        from zope.component import createObject
        object = createObject('faborder')
        object.id = data['id']
        object.title = data['name']
        user = self.context.portal_membership.getAuthenticatedMember()
        object.email = user.getProperty('email')
        object.exclude_from_nav = True
        object.reindexObject()
        return object
    
    def add(self, object):
        self.context._setObject(object.id, object)
        o = getattr(self.context, object.id)
        o.reindexObject()

    def render(self):
        """create faborder instance and redirect to its default view
        """
        
        # generate a nice order-number
        length = 8 # order number length
        chars = list(string.digits) # possible chars in order numbers
        # chars = chars, list(string.ascii_lowercase) # (add letters)
        # chars = sum(chars, []) # (flatten list)
        n = "".join(random.sample(chars, length))
        while self.context.hasObject(n):
            n = "".join(random.sample(chars, length))
        
        instance = self.create({
            'id': n, 
            'name': u"Fritzing Fab order %s" % (n)})
        self.add(instance)
        
        faborderURL = self.context.absolute_url()+"/"+instance.id
        self.request.response.redirect(faborderURL)

