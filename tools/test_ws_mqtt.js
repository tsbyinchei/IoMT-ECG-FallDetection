const https = require('https');

// Send raw MQTT CONNECT packet over WebSocket
// Let's see what the server responds with!

const req = https.request({
  hostname: 'mqtt.tsbyin.dev',
  port: 443,
  path: '/mqtt',
  headers: {
    'Connection': 'Upgrade',
    'Upgrade': 'websocket',
    'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
    'Sec-WebSocket-Version': '13',
    'Sec-WebSocket-Protocol': 'mqtt'
  }
});

req.on('upgrade', (res, socket, head) => {
  console.log('[1] WS connected! Status:', res.statusCode);

  // Build MQTT 3.1.1 CONNECT packet
  // Variable header: Protocol Name "MQTT" (4 bytes), Level 4, Flags, Keepalive 60
  // Payload: ClientID "TestClient"
  // Username: "TsByin", Password: "Chei@@@182728"
  
  const protoName = Buffer.from([0x00, 0x04, 0x4d, 0x51, 0x54, 0x54]); // "MQTT"
  const protoLevel = Buffer.from([0x04]); // 3.1.1
  const connectFlags = Buffer.from([0xc2]); // user + pass + clean session (11000010)
  const keepAlive = Buffer.from([0x00, 0x3c]); // 60s
  
  const clientId = Buffer.from('TestNode123');
  const clientIdField = Buffer.concat([Buffer.from([0x00, clientId.length]), clientId]);
  
  const user = Buffer.from('TsByin');
  const userField = Buffer.concat([Buffer.from([0x00, user.length]), user]);
  
  const pass = Buffer.from('Chei@@@182728');
  const passField = Buffer.concat([Buffer.from([0x00, pass.length]), pass]);
  
  const variableHeader = Buffer.concat([protoName, protoLevel, connectFlags, keepAlive]);
  const payload = Buffer.concat([clientIdField, userField, passField]);
  
  const remainingLength = variableHeader.length + payload.length;
  const fixedHeader = Buffer.from([0x10, remainingLength]); // 0x10 = CONNECT
  
  const mqttPacket = Buffer.concat([fixedHeader, variableHeader, payload]);
  
  // Wrap into WebSocket binary frame (masked, because client to server must be masked)
  // Fin=1, Opcode=2 (binary): 0x82
  const maskKey = Buffer.from([0x12, 0x34, 0x56, 0x78]);
  const maskedData = Buffer.alloc(mqttPacket.length);
  for (let i = 0; i < mqttPacket.length; i++) {
    maskedData[i] = mqttPacket[i] ^ maskKey[i % 4];
  }
  
  let wsFrame;
  if (mqttPacket.length < 126) {
    const b1 = 0x82;
    const b2 = 0x80 | mqttPacket.length; // Mask bit = 1
    wsFrame = Buffer.concat([Buffer.from([b1, b2]), maskKey, maskedData]);
  } else {
    const b1 = 0x82;
    const b2 = 0x80 | 126;
    const lenBuf = Buffer.from([ (mqttPacket.length >> 8) & 0xff, mqttPacket.length & 0xff ]);
    wsFrame = Buffer.concat([Buffer.from([b1, b2]), lenBuf, maskKey, maskedData]);
  }
  
  console.log('[2] Sending WebSocket MQTT CONNECT frame (' + wsFrame.length + ' bytes)...');
  socket.write(wsFrame);
  
  socket.on('data', (data) => {
    console.log('[3] Received from server (' + data.length + ' bytes):', data);
    // Unmask if needed, but server to client is not masked
    // Let's parse WS frame
    if (data.length >= 2) {
      const opcode = data[0] & 0x0f;
      console.log('WS opcode:', opcode, '(2=binary, 8=close, 9=ping)');
      if (opcode === 8) {
        console.log('Server sent CLOSE frame!');
        if (data.length >= 4) {
          const code = data.readUInt16BE(2);
          console.log('Close code:', code);
        }
      } else if (opcode === 2) {
        // Binary frame - likely MQTT CONNACK!
        let offset = 2;
        let len = data[1] & 0x7f;
        if (len === 126) offset = 4;
        const mqttData = data.slice(offset);
        console.log('MQTT packet type:', (mqttData[0] >> 4), '(2=CONNACK)');
        console.log('CONNACK return code:', mqttData[3]);
      }
    }
    socket.end();
  });
  
  setTimeout(() => {
    console.log('[Timeout] No response from server after 5s');
    socket.end();
  }, 5000);
});

req.on('error', (err) => {
  console.error('[Error]', err);
});

req.end();
