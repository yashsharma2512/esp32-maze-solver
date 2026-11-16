import ast
import sys
import types
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

source = (Path(__file__).parents[1] / "main.py").read_text()
tree = ast.parse(source)
keep = {"field", "density", "classify_letter", "ring_color", "read_ring_bands",
        "classify_ring", "find_ring_candidates", "open_camera_uart"}
keep_classes = {"TargetReporter"}
nodes = [n for n in tree.body if isinstance(n, (ast.FunctionDef, ast.ClassDef)) and n.name in keep | keep_classes]
code = compile(ast.Module(body=nodes, type_ignores=[]), "<nicla-rules>", "exec")
ns = {
    "RING_VALUES": {"red": -1, "green": 1, "blue": 2, "black": -2, "none": 0},
    "VICTIM_NAMES": {2: "HARMED", 1: "STABLE", 0: "UNHARMED"},
    "RING_SEARCH_SCALE": .5, "RING_SEARCH_DIVISOR": 2,
    "COLOR_THRESHOLDS": [],
    "time": types.SimpleNamespace(ticks_ms=lambda: 0, ticks_diff=lambda a,b:a-b),
}
exec(code, ns)

class Stats:
    def __init__(self, value): self.mean = value

class ImageMock:
    def __init__(self, image): self.image = image
    def get_statistics(self, roi):
        x,y,w,h = roi
        crop = self.image.crop((x,y,x+w,y+h))
        return Stats(sum(crop.get_flattened_data()) / (crop.width*crop.height))
    def get_pixel(self, point):
        return self.image.getpixel(point)

class Blob:
    def __init__(self, rect): self.rect=rect

results={}
for size in (70, 90, 110, 130):
    font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", size)
    for char, expected in (("Φ","PHI"),("Ψ","PSI"),("Ω","OMEGA"),
                       ("O",None),("H",None),("X",None),("S",None)):
        im=Image.new("L",(160,160),0)
        draw=ImageDraw.Draw(im)
        box=draw.textbbox((0,0),char,font=font)
        draw.text(((160-(box[2]-box[0]))//2-box[0],(160-(box[3]-box[1]))//2-box[1]),char,fill=255,font=font)
        rect=im.getbbox()
        got=ns["classify_letter"](ImageMock(im),Blob((rect[0],rect[1],rect[2]-rect[0],rect[3]-rect[1])))
        results[(char,size)]=got
        assert got==expected,(char,size,got,expected)
print("Greek glyphs and four non-target glyphs at four sizes passed")

for samples, expected in (
    (["red","green","green","none","none"],1),
    (["red","green","blue","none","none"],2),
    (["red","green","blue","black","none"],0),
    (["black"]*5,None),
    (["green"]*5,None),
    (["none"]*5,None),
):
    got=ns["classify_ring"](samples)
    assert got==expected,(samples,got,expected)
print("Ring scoring: 3 targets and 3 false candidates passed")

reporter=ns["TargetReporter"](-1)
assert reporter.packet(None,100)=="-1,0,-1,-1\n"
assert reporter.packet((2,120,80),130)=="2,0,60,40\n"
assert reporter.packet((2,120,80),180)=="2,50,60,40\n"
print("UART packet format: clear, target, stable age passed")

calls=[]
class UARTMock:
    def __init__(self,bus,baud):
        calls.append((bus,baud))
machine=types.ModuleType("machine")
machine.UART=UARTMock
sys.modules["machine"]=machine
assert ns["open_camera_uart"]() is not None
assert calls==[(4,115200)],calls
print("SNS ESLOV UART selection: bus 4 at 115200 passed")

# A square dark Greek character is a blob candidate, but is not a colored ring.
class SearchMock(ImageMock):
    def __init__(self, image, rect):
        super().__init__(image)
        self.rect=rect
    def width(self): return self.image.width
    def height(self): return self.image.height
    def draw_image(self,*args,**kwargs): pass
    def find_blobs(self,*args,**kwargs): return [Blob(self.rect)]

rect=(40,20,80,80)
dark=SearchMock(Image.new("RGB",(160,120),(0,0,0)),rect)
assert ns["find_ring_candidates"](dark,dark)==[]

# A synthetic band pattern survives the ring filter even with one noisy row.
rgb=Image.new("RGB",(160,120),(255,255,255))
points=(int(80-80*4.32/10),int(80-80*3.32/10),
        int(80-80*2.32/10),int(80-80*1.32/10),80)
for x,color in zip(points,[(230,20,20),(20,200,20),(20,20,230),
                           (255,255,255),(255,255,255)]):
    for y in (53,60,66):
        ImageDraw.Draw(rgb).rectangle((x-2,y-2,x+2,y+2),fill=color)
# Corrupt the center row at one band; the other two rows should win.
ImageDraw.Draw(rgb).rectangle((points[0]-2,58,points[0]+2,62),fill=(255,255,255))
search=SearchMock(rgb,rect)
rings=ns["find_ring_candidates"](search,search)
assert len(rings)==1,rings
assert ns["classify_ring"](rings[0][1][0])==2,rings
print("Simulated dark glyph rejected as ring; noisy colored ring retained")
