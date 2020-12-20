﻿#pragma once

#include <string>
#include <unknwn.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include "FileHelper.h"

namespace SampleHelper
{
  // Convert SoftwareBitmap to std::vector<float>
  std::vector<float> SoftwareBitmapToFloatVector(
    winrt::Windows::Graphics::Imaging::SoftwareBitmap softwareBitmap);

  // Create input Tensorfloats with 3 images.
  winrt::Windows::AI::MachineLearning::TensorFloat CreateInputTensorFloat();

  // Create input VideoFrames with 3 images
  winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Media::VideoFrame> CreateVideoFrames();

  winrt::hstring GetModelPath(std::string modelType);

  void PrintResults(winrt::Windows::Foundation::Collections::IVectorView<float> results);

}