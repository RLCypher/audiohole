"""Optional offline Chromium smoke checks for full generation review artifacts."""
from html.parser import HTMLParser
from io import BytesIO
import json
import base64
from pathlib import Path
import zipfile

from PIL import Image, ImageChops

from avatar_assets import ROOT, review_key


def main():
    from playwright.sync_api import sync_playwright
    results=[];errors=[]
    root=ROOT/'review/full'
    with sync_playwright() as p:
        browser=p.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox'])
        context=browser.new_context(offline=True,viewport={'width':1180,'height':1000})
        catalog=context.new_page();requests=[]
        catalog.on('request',lambda req:requests.append(req.url))
        catalog.on('pageerror',lambda e:errors.append(str(e)))
        catalog.set_content((root/'index.html').read_text(),wait_until='load')
        catalog.wait_for_function('document.images.length === 52 && Array.from(document.images).every(image => image.complete && image.naturalWidth === 128)')
        assert not any(not url.startswith('data:') for url in requests)
        results.append('catalog renders all 52 generated expressions offline')
        catalog.screenshot(path=str(root/'catalog-screenshot.png'),full_page=True)
        catalog.close()
        selections=[('default','surprised'),('uwu','talking'),('uwu','working'),('uwu','celebrating'),('uwu','panicking'),('uwu','deadpan'),('vanilla','error'),('vanilla','sleeping')]
        for pid,state in selections:
            page=context.new_page();page.on('pageerror',lambda e:errors.append(str(e)))
            page.set_content((root/review_key(pid)/f'{state}-preview.html').read_text(),wait_until='load')
            page.wait_for_function('document.getElementById("animation").complete && document.getElementById("animation").naturalWidth === 192')
            page.check('#reduced');page.wait_for_function('document.getElementById("animation").naturalWidth === 128')
            page.uncheck('#reduced');page.click('#replay')
            page.wait_for_function('document.getElementById("animation").complete && document.getElementById("animation").naturalWidth === 192')
            page.close()
        results.append('representative expressive, theatrical and static previews support replay and reduced motion')
        # Check actual timed GIF playback, finite rest, and explicit replay.
        page=context.new_page();page.set_content((root/'horu/celebrating-preview.html').read_text(),wait_until='load')
        page.wait_for_function('document.getElementById("animation").complete')
        image=page.locator('#animation')
        first=image.screenshot();page.wait_for_timeout(750);peak=image.screenshot()
        assert ImageChops.difference(Image.open(BytesIO(first)).convert('RGB'),Image.open(BytesIO(peak)).convert('RGB')).getbbox()
        page.wait_for_timeout(1900);rest=image.screenshot();page.click('#replay');page.wait_for_timeout(750);replay=image.screenshot()
        assert ImageChops.difference(Image.open(BytesIO(rest)).convert('RGB'),Image.open(BytesIO(replay)).convert('RGB')).getbbox(),'Replay did not restart finite GIF'
        results.append('finite Horu celebration visibly advances, returns to rest, and replays')
        page.close()
        demo=context.new_page()
        encoded=base64.b64encode((root/'horu-body-revision-loop.gif').read_bytes()).decode()
        demo.set_content(f'<img id="demo" src="data:image/gif;base64,{encoded}">',wait_until='load')
        demo.wait_for_function('document.getElementById("demo").complete')
        first=demo.locator('#demo').screenshot();demo.wait_for_timeout(750);acting=demo.locator('#demo').screenshot()
        assert ImageChops.difference(Image.open(BytesIO(first)).convert('RGB'),Image.open(BytesIO(acting)).convert('RGB')).getbbox()
        demo.close()
        results.append('standalone three-state body demo decodes and visibly animates offline')
        for pid,state in [('default','talking'),('uwu','talking'),('uwu','working'),('uwu','error'),('vanilla','working')]:
            page=context.new_page();page.on('pageerror',lambda e:errors.append(str(e)))
            page.set_content((root/review_key(pid)/f'{state}-inspector.html').read_text(),wait_until='load')
            page.wait_for_function('document.getElementById("avatar").complete && document.getElementById("avatar").naturalWidth === 512')
            assert page.locator('#state').input_value()==state
            if pid=='default':
                assert page.locator('#personality option').all_text_contents()==['Norm']
                assert page.locator('#summary').inner_text().startswith('Norm /')
            elif pid=='uwu':
                assert page.locator('#personality option').all_text_contents()==['Horu']
                assert page.locator('#summary').inner_text().startswith('Horu /')
            page.check('#reduced');assert 'reduced_motion_static' in page.locator('#summary').inner_text()
            page.uncheck('#reduced');page.click('#step');assert 'Frame 2 /' in page.locator('#frame').inner_text()
            page.check('#approved')
            if pid=='uwu':assert page.locator('#avatar').is_hidden()
            else:assert page.evaluate('active.resolved_state') == state
            page.uncheck('#approved');page.check('#missing');assert page.locator('#avatar').is_hidden()
            page.uncheck('#missing');page.select_option('#display','128');assert page.locator('#avatar').bounding_box()['width']==128
            page.select_option('#display','512');assert page.locator('#avatar').bounding_box()['width']==512
            page.close()
        results.append('state inspectors select requested states, step frames, filter approval, hide missing assets and scale correctly')
        assert not errors,errors
        context.close();browser.close()
    # Review ZIP links must work after extraction; native PNGs intentionally
    # remain in the repository and are not linked from the compact ZIP index.
    class Links(HTMLParser):
        def __init__(self):super().__init__();self.links=[]
        def handle_starttag(self,tag,attrs):
            for name,value in attrs:
                if tag=='a' and name=='href':self.links.append(value)
    for pid in ['default','uwu','vanilla']:
        with zipfile.ZipFile(root/f'{review_key(pid)}-review.zip') as archive:
            parser=Links();parser.feed(archive.read('index.html').decode())
            names=set(archive.namelist())
            assert all(link in names for link in parser.links),(pid,[link for link in parser.links if link not in names])
    results.append('portable ZIP catalog links resolve entirely within each archive')
    report={'browser':'local Chromium, offline','passed':results,'errors':errors,
            'scope':'Workspace browser verification; Codex embedded visualization host is not directly testable.'}
    (root/'browser-checks.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
