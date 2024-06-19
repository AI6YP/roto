'use strict';

const stringify = require('onml/stringify.js');
const tt = require('onml/tt.js');
const genSvg = require('onml/gen-svg.js');

const getMl = (data) => {
  const { valElev, tgtElev, valAzim, tgtAzim } = data;
  const svgE = genSvg(256, 256);
  svgE[1].class = 'sat';
  svgE.push(['g', tt(32, 224),
    ['circle', {r: 192, style: 'fill:none;stroke:#555'}],
    ['path', {d: `M 0 0 L ${
      192 * Math.cos(2 * Math.PI * tgtElev - 0.1)
    } ${
      -192 * Math.sin(2 * Math.PI * tgtElev - 0.1)
    } L ${
      192 * Math.cos(2 * Math.PI * tgtElev + 0.1)
    } ${
      -192 * Math.sin(2 * Math.PI * tgtElev + 0.1)
    } z`, fill: '#161'}],
    ['line', {
      stroke: '#1f1', 'stroke-width': 3,
      x1: 0,
      y1: 0,
      x2: 192 * Math.cos(2 * Math.PI * tgtElev),
      y2: -192 * Math.sin(2 * Math.PI * tgtElev)
    }],
    ...Array.from({length: 7}, (e, i) => ['circle', {
      r:5,
      cx: 192 * Math.sin(2 * Math.PI * i / 24),
      cy: -192 * Math.cos(2 * Math.PI * i / 24),
      style: 'fill:#aaa;'
    }]),
    ...Array.from({length: 19}, (e, i) => ['circle', {
      r:2,
      cx: 192 * Math.sin(2 * Math.PI * i / 72),
      cy: -192 * Math.cos(2 * Math.PI * i / 72)
    }]),
    ...Array.from({length: 7}, (e, i) => ['text', {
      x: 210 * Math.cos(2 * Math.PI * i / 24),
      y: -210 * Math.sin(2 * Math.PI * i / 24) + 6,
      'text-anchor': 'middle',
      'font-size': 16
    }, i * 15]),
    ['line', {
      stroke: '#1af', 'stroke-width': 3,
      x1: 0,
      y1: 0,
      x2: 192 * Math.cos(2 * Math.PI * valElev),
      y2: -192 * Math.sin(2 * Math.PI * valElev)
    }],
    ['text', {'text-anchor': 'middle', 'font-size': 24, x: 64, y: -50}, (tgtElev * 360).toFixed(1)],
    ['text', {'text-anchor': 'middle', 'font-size': 24, x: 64, y: -10}, (valElev * 360).toFixed(1)]
  ]);

  const svgA = genSvg(256, 256);
  svgA[1].class = 'sat';
  svgA.push(['g', tt(128, 128),
    ['circle', {r: 100, style: 'fill:none;stroke:#555'}],
    ['path', {d: `M 0 0 L ${
      100 * Math.sin(2 * Math.PI * tgtAzim - 0.1)
    } ${
      -100 * Math.cos(2 * Math.PI * tgtAzim - 0.1)
    } L ${
      100 * Math.sin(2 * Math.PI * tgtAzim + 0.1)
    } ${
      -100 * Math.cos(2 * Math.PI * tgtAzim + 0.1)
    } z`, fill: '#161'}],
    ...Array.from({length: 12}, (e, i) => ['circle', {
      r:5,
      cx: 100 * Math.sin(2 * Math.PI * i / 12),
      cy: 100 * Math.cos(2 * Math.PI * i / 12),
      style: 'fill:#aaa;'
    }]),
    ...Array.from({length: 36}, (e, i) => ['circle', {
      r:2,
      cx: 100 * Math.sin(2 * Math.PI * i / 36),
      cy: 100 * Math.cos(2 * Math.PI * i / 36)
    }]),
    ...Array.from({length: 12}, (e, i) => ['text', {
      x: 116 * Math.sin(2 * Math.PI * i / 12),
      y: -116 * Math.cos(2 * Math.PI * i / 12) + 6,
      'text-anchor': 'middle',
      'font-size': 16
    }, i * 30]),
    ['line', {
      stroke: '#1f1', 'stroke-width': 3,
      x1: 0,
      y1: 0,
      x2: 100 * Math.sin(2 * Math.PI * tgtAzim),
      y2: -100 * Math.cos(2 * Math.PI * tgtAzim)
    }],
    ['line', {
      stroke: '#1af', 'stroke-width': 3,
      x1: 0,
      y1: 0,
      x2: 100 * Math.sin(2 * Math.PI * valAzim),
      y2: -100 * Math.cos(2 * Math.PI * valAzim)
    }],
    ['text', {'text-anchor': 'middle', 'font-size': 24, x: -50, y: 10}, (tgtAzim * 360).toFixed(1)],
    ['text', {'text-anchor': 'middle', 'font-size': 24, x:  50, y: 10}, (valAzim * 360).toFixed(1)]
  ]);
  return ['div',
    ['style', '.sat { background: #222; fill: #fff; font-size: 16px; font-family: "Iosevka Drom"}'],
    svgA, svgE
  ];
};

global.ROTODROM = async (divName) => {
  const content = document.getElementById(divName);

  const font = new FontFace('Iosevka Drom', 'url(https://vc.drom.io/IosevkaDrom-Regular.woff2)');
  document.fonts.add(font);
  await font.load();

  // const state = {};

  const socket = new WebSocket('ws://' + location.host + '/dev1');
  socket.binaryType = 'arraybuffer';

  // Connection opened
  await new Promise((resolve, reject) => {
    socket.addEventListener('open', (event) => {
      resolve();
    });
  });

  // Listen for messages
  socket.addEventListener('message', (event) => {
    // const u8 = new Uint8Array(event.data);
    const u32 = new Int32Array(event.data);
    console.log('Message from server: <', u32, '>');
    const ml = getMl({
      valAzim: u32[0] / 360000, tgtAzim: u32[4] / 360000,
      valElev: u32[1] / 360000, tgtElev: u32[5] / 360000
    });
    content.innerHTML = stringify(ml);
  });

  global.SOCKET = {
    send: (msg) => {
      socket.send(msg);
    }
  };

  setInterval(() => {
    socket.send(Uint8Array.from([1]));
  }, 200);

};

/* eslint-env browser */
