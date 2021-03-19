
import 'dart:async';

import 'package:flutter/services.dart';

class Texture {
  static const MethodChannel _channel =
      const MethodChannel('textureFactory');

  static Future<String> get platformVersion async {
    final String version = await _channel.invokeMethod('getPlatformVersion');
    return version;
  }

  static Future<int> newTexture(int width, int height) async {
    final int texId = await _channel.invokeMethod('newTexture', {
      'width':  width,
      'height': height
    });
    return texId;
  }
}
