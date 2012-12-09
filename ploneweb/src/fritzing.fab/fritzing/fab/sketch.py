import zipfile
from cStringIO import StringIO

from five import grok
from zope.lifecycleevent.interfaces import IObjectCreatedEvent, IObjectModifiedEvent

from fritzing.fab.interfaces import ISketch
from fritzing.fab import getboardsize


@grok.subscribe(ISketch, IObjectCreatedEvent)
@grok.subscribe(ISketch, IObjectModifiedEvent)
def modifiedHandler(sketch, event):
	"""update the sketch size calculation
		(this is after the file has been validated)
	"""
	fzzName = sketch.orderItem.filename
	fzzData = StringIO(sketch.orderItem.data)
	zf = zipfile.ZipFile(fzzData)
	pairs = getboardsize.fromZipFile(zf, fzzName)
	sketch.boards = [
		{'width':pairs[i] / 10, 'height':pairs[i+1] / 10} 
		for i in range(0, len(pairs), 2)
	]
	sketch.area = 0
	for board in sketch.boards:
		sketch.area += board['width'] * board['height']
