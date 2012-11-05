from five import grok

from fritzing.fab.interfaces import ISketch

from zope.lifecycleevent.interfaces import IObjectCreatedEvent, IObjectModifiedEvent


@grok.subscribe(ISketch, IObjectCreatedEvent)
@grok.subscribe(ISketch, IObjectModifiedEvent)
def modifiedHandler(sketch, event):
    sketch.boards = sketch.orderItem.boards
    sketch.area = 0
    for board in sketch.boards:
        sketch.area += board['width'] * board['height']
