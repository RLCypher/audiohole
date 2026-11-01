"""Optional browser smoke check: uses installed Playwright and local Chromium.

Not a mandatory pipeline dependency; no browser or package downloads are made.
"""
import argparse
import json
from io import BytesIO
from pathlib import Path

from avatar_assets import ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chromium', default='/usr/bin/chromium')
    args = parser.parse_args()
    from playwright.sync_api import sync_playwright
    checks = []
    with sync_playwright() as p:
        browser = p.chromium.launch(executable_path=args.chromium, headless=True, args=['--no-sandbox'])
        page = browser.new_page(viewport={'width': 1180, 'height': 1000}, device_scale_factor=1)
        errors = []
        page.on('pageerror', lambda error: errors.append(str(error)))
        # Managed Chromium blocks file:// navigation; this self-contained page
        # can be loaded directly without a server or any network requests.
        page.set_content((ROOT / 'review/preview.html').read_text(), wait_until='load')
        page.locator('#avatar').wait_for(state='visible')
        page.wait_for_function('document.getElementById("avatar").naturalWidth === 512')
        assert page.locator('#avatar').evaluate('(img) => img.naturalHeight === 768')
        checks.append('idle decoded at 512x768')
        for identity in ['default', 'uwu', 'vanilla']:
            page.select_option('#personality', identity)
            page.select_option('#state', '__unknown__')
            assert 'idle_animation' in page.locator('#summary').inner_text()
            assert identity in page.locator('#summary').inner_text()
        checks.append('unknown state falls back within each personality')
        page.check('#missing')
        assert page.locator('#avatar').is_hidden()
        page.select_option('#personality', 'uwu')
        assert page.locator('#avatar').is_hidden()
        page.uncheck('#missing')
        assert page.locator('#avatar').is_visible()
        checks.append('missing pack and personality switches hide correctly')
        page.check('#approved')
        assert page.locator('#avatar').is_visible()
        assert 'approved' in page.locator('#summary').inner_text()
        page.uncheck('#approved')
        assert page.locator('#avatar').is_visible()
        checks.append('production approval filter retains the accepted IDLE baseline')
        page.select_option('#style', 'regular')
        assert page.locator('#avatar').is_hidden()
        page.select_option('#style', 'chibi')
        assert page.locator('#avatar').is_visible()
        checks.append('disabled regular style cannot borrow chibi art')
        for _ in range(3):
            page.click('#background')
        page.check('#guides')
        assert 'guides' in page.locator('#stage').get_attribute('class')
        checks.append('all backgrounds and guide toggle')
        page.select_option('#display', '128')
        assert page.locator('#avatar').bounding_box()['width'] == 128
        page.select_option('#display', '512')
        assert page.locator('#avatar').bounding_box()['width'] == 512
        checks.append('native 512x768 and compact 128x192 display selectors')
        assert page.evaluate('active.frames.length') == 12
        assert page.evaluate('active.durations_ms.reduce((a,b)=>a+b,0)') == 5000
        page.evaluate('index=4;render();play()')
        page.wait_for_function('() => { if(index === 5) { stop(); return true; } return false; }')
        assert 'Frame 6 / 12' in page.locator('#frame').inner_text()
        page.click('#pause')
        page.click('#step')
        assert 'Frame 7 / 12' in page.locator('#frame').inner_text()
        page.evaluate('index=11;play()')
        page.wait_for_function('() => { if(index === 0) { stop(); return true; } return false; }')
        checks.append('actual 12-frame playback, blink frame, pause, step and loop wrap')
        page.evaluate('active.loop=false;index=11;play()')
        page.wait_for_function('document.getElementById("state").value === "idle" && index === 0')
        checks.append('one-shot timer returns to idle')
        page.select_option('#personality', 'uwu')
        page.screenshot(path=str(ROOT / 'review/preview-screenshot.png'), full_page=True)
        assert not errors, errors
        checks.append('no browser JavaScript errors')
        gallery_context = browser.new_context(offline=True,viewport={'width':1180,'height':850})
        gallery = gallery_context.new_page()
        requests = []
        gallery.on('request',lambda request: requests.append(request.url))
        gallery_errors = []
        gallery.on('pageerror',lambda error: gallery_errors.append(str(error)))
        gallery.set_content((ROOT/'review/idle-sequences/index.html').read_text(),wait_until='load')
        gallery.wait_for_function('document.images.length === 3 && Array.from(document.images).every(image => image.complete && image.naturalWidth === 256)')
        assert not any(not url.startswith('data:') for url in requests),requests
        assert not gallery_errors,gallery_errors
        checks.append('standalone gallery decodes all GIFs offline with no neighboring files or scripts')
        from PIL import Image, ImageChops
        before = [image.screenshot() for image in gallery.locator('img').all()]
        gallery.wait_for_timeout(850)
        after = [image.screenshot() for image in gallery.locator('img').all()]
        for a,b in zip(before,after):
            difference = ImageChops.difference(Image.open(BytesIO(a)).convert('RGB'),Image.open(BytesIO(b)).convert('RGB'))
            assert difference.getbbox(),'GIF did not visibly advance in the browser'
        checks.append('all three portable GIF previews visibly advance in Chromium')
        gallery.screenshot(path=str(ROOT/'review/idle-sequences/gallery-screenshot.png'),full_page=True)
        gallery_context.close()
        browser.close()
    result = {'browser': 'local Chromium via Playwright', 'passed': checks, 'errors': errors,
              'note': 'Playback checked against the actual 12-frame IDLE sequences and 5000ms timing.'}
    (ROOT / 'review/browser-checks.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
