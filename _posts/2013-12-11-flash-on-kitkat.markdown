---
layout: post
title: "Flash on Android 4.4 KitKat"
date: 2013-12-11 12:30:00
---

There has been some some talk recently about the Flash situation on Android 4.4. While it's no secret that Adobe discontinued support for Flash on Android [a while back](http://blogs.adobe.com/conversations/2011/11/flash-focus.html), there are still a lot of folks using it on a daily basis. The Firefox for Android team consistently gets feedback about it, so it didn't take long to find out that things were amiss on KitKat.

I looked into the problem a few weeks ago in [bug 935676](https://bugzilla.mozilla.org/show_bug.cgi?id=935676), and found that some reserved functions were [made virtual](http://androidxref.com/4.4_r1/xref/system/core/libpixelflinger/codeflinger/tinyutils/VectorImpl.h#103), breaking binary compatibility. I initially wanted to find a workaround that involved injecting the missing symbols, but that seems to be a bit of a dead end. I ended up making things work by unpacking the Flash APK with [apktool](http://code.google.com/p/android-apktool/), and modifying libflashplayer.so with a hex editor to replace the references to the missing symbols with something else. The functions in question aren't actually being called, so changing them to anything that exists works (I think I used `pipe`). It was necessary to pad each field with null characters to keep the size of the symbol table unchanged. After that I just repacked with apktool, installed, and everything seemed to work.

There is apparently an APK floating around that makes Flash work in other browsers on KitKat, but not Firefox. The above solution should allow someone to make an APK that works everywhere, so give it a shot if you are so inclined. I am not going to publish my own APK because of reasons.