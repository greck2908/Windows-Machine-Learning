#pragma once

#include "../pch.h"
#include "debug_cpu.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::AI::MachineLearning;
using namespace Windows::Media;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace std;

static const int CHANNELS = 1;
static const int HEIGHT = 2;
static const int WIDTH = 3;
static const int NUM_DIMENSIONS = 4;
static const int TARGET_OPSET = 7;

HRESULT DebugShapeInferrer::InferOutputShapes (IMLOperatorShapeInferenceContext* context) noexcept
{
    try
    {
        uint32_t inputDimsSize;
        context->GetInputTensorDimensionCount(0, &inputDimsSize);

        uint32_t *inputDims = new uint32_t[inputDimsSize];
        winrt::check_hresult(context->GetInputTensorShape(0, inputDimsSize, inputDims));

        winrt::check_hresult(context->SetOutputTensorShape(0, inputDimsSize, inputDims));
        return S_OK;
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
}

template <typename T>
void WriteToPng(vector<uint32_t> inputDims, T* inputData, uint32_t size, hstring m_filePath)
{    
    // expects nchw format
    if (inputDims.size() != NUM_DIMENSIONS) {
        return;
    }

    // Convert data into pixel bytes
    // TODO: add option to normalize data
    // currently if data values increase beyond the capacity of a byte then information will be lost
    vector<uint8_t> byteCopy;
    for (int i = 0; i < size; i++) {
        byteCopy.push_back( static_cast<uint8_t>(inputData[i]));
    }

    // Get current directory
    wchar_t buf[MAX_PATH];
    _wgetcwd(buf, 256);
    StorageFolder parentFolder = StorageFolder::GetFolderFromPathAsync(buf).get();
    int pixelsPerImage = inputDims.at(HEIGHT) * inputDims.at(WIDTH);

    // for each output channel at this point in the network
    for (int i = 0; i < inputDims.at(CHANNELS); i++) {
        // create png file
        size_t outSize = 0;
        wstring suffix = L"_" + to_wstring(i);
        wstring ext = L".png";
        wstring finalPath{ m_filePath };
        if (finalPath.compare(finalPath.size() - ext.size(), finalPath.size(), ext) != 0) {
            // append .png extension to filename
            finalPath += suffix;
            finalPath += ext;
        }
        else {
            finalPath = finalPath.substr(0, finalPath.size() - ext.size()) + suffix + ext;
        }

        StorageFile outputFile = parentFolder.CreateFileAsync(finalPath, CreationCollisionOption::ReplaceExisting).get();
        IRandomAccessStream stream = outputFile.OpenAsync(FileAccessMode::ReadWrite).get();
        BitmapEncoder encoder = BitmapEncoder::CreateAsync(BitmapEncoder::PngEncoderId(), stream).get();

        // select image pixels
        vector<uint8_t> sub_vector(byteCopy.begin() + (i * pixelsPerImage), byteCopy.begin() + ((i + 1) * pixelsPerImage));
        DataWriter writer;
        writer.WriteBytes(sub_vector);
        SoftwareBitmap softwareBitmap(BitmapPixelFormat::Gray8, inputDims.at(HEIGHT), inputDims.at(WIDTH));
        IBuffer buffer = writer.DetachBuffer();
        softwareBitmap.CopyFromBuffer(buffer);
        // target pixel format is arbitrary because each channel will have equal value
        encoder.SetSoftwareBitmap(SoftwareBitmap::Convert(softwareBitmap, BitmapPixelFormat::Bgra8));
        encoder.FlushAsync().get();
        stream.Close();
    }
}

template <typename T>
void WriteToText(vector<uint32_t> inputDims, T* inputData, uint32_t size, hstring m_filePath, MLOperatorTensorDataType dataType) {
    // Get current directory
    wchar_t buf[MAX_PATH];
    _wgetcwd(buf, 256);
    StorageFolder parentFolder = StorageFolder::GetFolderFromPathAsync(buf).get();
    parentFolder.CreateFileAsync(m_filePath, CreationCollisionOption::ReplaceExisting).get();

    ofstream outputFile;
    outputFile.open(winrt::to_string(m_filePath));
    outputFile << "dimensions: ";
    std::copy(begin(inputDims), end(inputDims), std::ostream_iterator<uint32_t>(outputFile, ", "));
    outputFile << "\ndata type: ";
    outputFile << (int)dataType;
    outputFile << "\ndata: ";
    for (int i = 0; i < size; i++) {
        outputFile << inputData[i];
        outputFile << ", ";
    }
    outputFile.close();
}

template <typename T>
void ComputeInternal(IMLOperatorTensor* pInputTensor, IMLOperatorTensor* pOutputTensor, uint32_t size,
                    vector<uint32_t> inputDims, hstring m_filePath, hstring m_fileType)
{
    // Just copy the data from output to input
    // Then print the input out to a file
    auto inputData = static_cast<T*>(pInputTensor->GetData());
    auto outputData = static_cast<T*>(pOutputTensor->GetData());
    auto dataType = pInputTensor->GetTensorDataType();
    
    if (to_string(m_fileType) == "png") {
        WriteToPng(inputDims, inputData, size, m_filePath);
    }
    else if (winrt::to_string(m_fileType) == "text") {
        WriteToText(inputDims, inputData, size, m_filePath, dataType);
    }
    // only useful if the debug output is used for some reason 
    // (not necessary since debug output can be consumed by no nodes without changing model execution
    memcpy(outputData, inputData, size);

}


// Computes the outputs of the kernel.  This may be called multiple times
// simultaneously within the same instance of the class.  Implementations
// of this method must be thread-safe.
HRESULT DebugOperator::Compute(IMLOperatorKernelContext* context)
{
    try
    {
        // Get the input tensor
        winrt::com_ptr<IMLOperatorTensor> inputTensor;
        winrt::check_hresult(context->GetInputTensor(0, inputTensor.put()));

        // Get the output tensor
        winrt::com_ptr<IMLOperatorTensor> outputTensor;
        winrt::check_hresult(context->GetOutputTensor(0, outputTensor.put()));

        // Get the input and output shape sizes
        uint32_t inputDimsSize = inputTensor->GetDimensionCount();
        uint32_t outputDimsSize = outputTensor->GetDimensionCount();
        if (inputDimsSize != outputDimsSize)
        {
            return E_UNEXPECTED;
        }

        // Get the input shape
        std::vector<uint32_t> inputDims(inputDimsSize);
        winrt::check_hresult(inputTensor->GetShape(inputDimsSize, inputDims.data()));

        // Get the output shape
        std::vector<uint32_t> outputDims(outputDimsSize);
        winrt::check_hresult(outputTensor->GetShape(outputDimsSize, outputDims.data()));

        // For the number of total elements in the input and output shapes
        auto outputDataSize = std::accumulate(outputDims.begin(), outputDims.end(), 1, std::multiplies<uint32_t>());
        auto inputDataSize = std::accumulate(inputDims.begin(), inputDims.end(), 1, std::multiplies<uint32_t>());
        if (outputDataSize != inputDataSize)
        {
            return E_UNEXPECTED;
        }

        MLOperatorTensorDataType type = inputTensor->GetTensorDataType();

        if (outputTensor->GetTensorDataType() != type) {
            return E_UNEXPECTED;
        }

        if (outputTensor->IsCpuData() && inputTensor->IsCpuData()) {

                switch (type) {
                    case MLOperatorTensorDataType::Float:
                    case MLOperatorTensorDataType::Float16:
                        ComputeInternal<float>(inputTensor.get(), outputTensor.get(), inputDataSize, inputDims, m_filePath, m_fileType);
                        break;
                    case MLOperatorTensorDataType::Bool:
                        ComputeInternal<bool>(inputTensor.get(), outputTensor.get(), inputDataSize, inputDims, m_filePath, m_fileType);
                        break;
                    case MLOperatorTensorDataType::Double:
                        ComputeInternal<double>(inputTensor.get(), outputTensor.get(), inputDataSize, inputDims, m_filePath, m_fileType);
                        break;
                    case MLOperatorTensorDataType::UInt8:
                        ComputeInternal<unsigned char>(inputTensor.get(), outputTensor.get(), inputDataSize, inputDims, m_filePath, m_fileType);
                        break;
                    case MLOperatorTensorDataType::Int8:
                        ComputeInternal<char>(inputTensor.get(), outputTensor.get(), inputDataSize, inputDims, m_filePath, m_fileType);
                        break;
                    case MLOperatorTensorDataType::UInt16:
                        ComputeInternal<unsigned short int>(inputTensor.get(), outputTensor.get(), inputDataSize, inputDims, m_filePath, m_fileType);
                        break;
                    case MLOperatorTensorDataType::Int16:
                        ComputeInternal<short int>(inputTensor.get(), outputTensor.get(), inputDataSize, inputDims, m_filePath, m_fileType);
                        break;
                    case MLOperatorTensorDataType::Int32:
                        ComputeInternal<int>(inputTensor.get(), outputTensor.get(), inputDataSize, inputDims, m_filePath, m_fileType);
                        break;
                    case MLOperatorTensorDataType::UInt32:
                        ComputeInternal<unsigned int>(inputTensor.get(), outputTensor.get(), inputDataSize, inputDims, m_filePath, m_fileType);
                        break;
                    case MLOperatorTensorDataType::Int64:
                        ComputeInternal<long long int>(inputTensor.get(), outputTensor.get(), inputDataSize, inputDims, m_filePath, m_fileType);
                        break;
                    case MLOperatorTensorDataType::UInt64:
                        ComputeInternal<unsigned long long int>(inputTensor.get(), outputTensor.get(), inputDataSize, inputDims, m_filePath, m_fileType);
                        break;
                }
        }
        return S_OK;
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
}

HRESULT DebugOperatorFactory::CreateKernel(
    IMLOperatorKernelCreationContext* context,
    IMLOperatorKernel** kernel)
{
    try
    {
        int filePathSize;
        context->GetStringAttributeElementLength("file_path", 0, reinterpret_cast<uint32_t*>(&filePathSize));
        char* filePath = new char[filePathSize];
        context->GetStringAttributeElement("file_path", 0, filePathSize, filePath);

        wchar_t* widePath = new wchar_t[strlen(filePath) + 1];
        size_t outSize = 0;
        mbstowcs_s(&outSize, widePath, strlen(filePath) + 1, filePath, _TRUNCATE);

        int fileTypeSize;
        context->GetStringAttributeElementLength("file_type", 0, reinterpret_cast<uint32_t*>(&fileTypeSize));
        char* fileType = new char[fileTypeSize];
        context->GetStringAttributeElement("file_type", 0, fileTypeSize, fileType);

        wchar_t* wideType = new wchar_t[strlen(fileType) + 1];
        mbstowcs_s(&outSize, wideType, strlen(fileType) + 1, fileType, _TRUNCATE);

        auto debugOperator = winrt::make<DebugOperator>(widePath, wideType);
        debugOperator.copy_to(kernel);
        delete[] filePath, fileType, widePath, wideType;
        return S_OK;
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
}

MLOperatorEdgeDescription DebugOperatorFactory::CreateEdgeDescriptor(MLOperatorEdgeType type, MLOperatorTensorDataType dataType)
{
    MLOperatorEdgeDescription desc;
    desc.edgeType = MLOperatorEdgeType::Tensor;
    desc.tensorDataType = dataType;
    return desc;
}

void DebugOperatorFactory::RegisterDebugSchema(winrt::com_ptr<IMLOperatorRegistry> registry)
{
    MLOperatorSetId operatorSetId;
    operatorSetId.domain = "";
    operatorSetId.version = TARGET_OPSET;

    MLOperatorSchemaDescription debugSchema = {};
    debugSchema.name = "Debug";
    debugSchema.operatorSetVersionAtLastChange = 1;

    MLOperatorSchemaEdgeDescription debugXInput;
    debugXInput.options = MLOperatorParameterOptions::Single;
    debugXInput.typeFormat = MLOperatorSchemaEdgeTypeFormat::Label;
    debugXInput.typeLabel = "T";

    std::vector<MLOperatorSchemaEdgeDescription> inputs{ debugXInput };
    debugSchema.inputs = inputs.data();
    debugSchema.inputCount = static_cast<uint32_t>(inputs.size());

    MLOperatorSchemaEdgeDescription debugXOutput;
    debugXOutput.options = MLOperatorParameterOptions::Single;
    debugXOutput.typeFormat = MLOperatorSchemaEdgeTypeFormat::Label;
    debugXOutput.typeLabel = "T";

    std::vector<MLOperatorSchemaEdgeDescription> outputs{ debugXOutput };
    debugSchema.outputs = outputs.data();
    debugSchema.outputCount = static_cast<uint32_t>(outputs.size());

    MLOperatorEdgeTypeConstrant typeConstraint;
    typeConstraint.typeLabel = "T";
    std::vector<MLOperatorEdgeDescription> allowedEdges
    {
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Float),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::UInt8),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Int8),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::UInt16),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Int16),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Int32),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Int64),
        //CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::String), documented as unsupported
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Bool),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Float16),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Double),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::UInt32),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::UInt64),
        //CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Complex64), documented as unsupported
        //CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Complex128) documented as unsupported
    };
    typeConstraint.allowedTypes = allowedEdges.data();
    typeConstraint.allowedTypeCount = static_cast<uint32_t>(allowedEdges.size());

    std::vector<MLOperatorEdgeTypeConstrant> typeConstraints{ typeConstraint };
    debugSchema.typeConstraints = typeConstraints.data();
    debugSchema.typeConstraintCount = static_cast<uint32_t>(typeConstraints.size());

    MLOperatorAttribute debugFilePathAttribute;
    debugFilePathAttribute.name = "file_path";
    debugFilePathAttribute.required = false;
    debugFilePathAttribute.type = MLOperatorAttributeType::String;

    MLOperatorAttribute debugFileTypeAttribute;
    debugFileTypeAttribute.name = "file_type";
    debugFileTypeAttribute.required = false;
    debugFileTypeAttribute.type = MLOperatorAttributeType::String;

    std::vector<MLOperatorAttribute> attributes{ debugFilePathAttribute, debugFileTypeAttribute };
    debugSchema.attributes = attributes.data();
    debugSchema.attributeCount = static_cast<uint32_t>(attributes.size());

    MLOperatorAttributeNameValue debugFilePathAttributeValue;
    debugFilePathAttributeValue.name = "file_path";
    debugFilePathAttributeValue.type = MLOperatorAttributeType::String;
    debugFilePathAttributeValue.valueCount = 1;
    static const char* defaultPaths[] = { "" };
    debugFilePathAttributeValue.strings = defaultPaths;

    MLOperatorAttributeNameValue debugFileTypeAttributeValue;
    debugFileTypeAttributeValue.name = "file_type";
    debugFileTypeAttributeValue.type = MLOperatorAttributeType::String;
    debugFileTypeAttributeValue.valueCount = 1;
    static const char* defaultTypes[] = { "png" };
    debugFileTypeAttributeValue.strings = defaultPaths;

    std::vector<MLOperatorAttributeNameValue> attributeDefaultValues{ debugFilePathAttributeValue, debugFileTypeAttributeValue };
    debugSchema.defaultAttributes = attributeDefaultValues.data();
    debugSchema.defaultAttributeCount = static_cast<uint32_t>(attributeDefaultValues.size());

    std::vector<const MLOperatorSchemaDescription*> schemas{ &debugSchema };
    registry->RegisterOperatorSetSchema(
        &operatorSetId,
        TARGET_OPSET /* baseline version */,
        schemas.data(),
        static_cast<uint32_t>(schemas.size()),
        nullptr,
        nullptr
    );
}


void DebugOperatorFactory::RegisterDebugKernel(winrt::com_ptr<IMLOperatorRegistry> registry)
{
    MLOperatorKernelDescription kernelDescription = {};
    kernelDescription.domain = "";
    kernelDescription.name = "Debug";
    kernelDescription.minimumOperatorSetVersion = 1;
    kernelDescription.executionType = MLOperatorExecutionType::Cpu;

    MLOperatorEdgeTypeConstrant typeConstraint;
    typeConstraint.typeLabel = "T";
    std::vector<MLOperatorEdgeDescription> allowedEdges
    {
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Float),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::UInt8),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Int8),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::UInt16),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Int16),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Int32),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Int64),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Bool),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Float16),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Double),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::UInt32),
        CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::UInt64)
    };
    typeConstraint.allowedTypes = allowedEdges.data();
    typeConstraint.allowedTypeCount = static_cast<uint32_t>(allowedEdges.size());

    std::vector<MLOperatorEdgeTypeConstrant> typeConstraints{ typeConstraint };
    kernelDescription.typeConstraints = typeConstraints.data();
    kernelDescription.typeConstraintCount = static_cast<uint32_t>(typeConstraints.size());

    MLOperatorAttributeNameValue debugFilePathAttributeValue;
    debugFilePathAttributeValue.name = "fail-path";
    debugFilePathAttributeValue.type = MLOperatorAttributeType::String;
    debugFilePathAttributeValue.valueCount = 1;
    static const char* defaultFilePath[] = { "" };
    debugFilePathAttributeValue.strings = defaultFilePath;

    std::vector<MLOperatorAttributeNameValue> attributeDefaultValues{ debugFilePathAttributeValue };
    kernelDescription.defaultAttributes = attributeDefaultValues.data();
    kernelDescription.defaultAttributeCount = static_cast<uint32_t>(attributeDefaultValues.size());
    kernelDescription.options = MLOperatorKernelOptions::None;
    kernelDescription.executionOptions = 0;

    auto factory = winrt::make<DebugOperatorFactory>();
    auto shareInferrer = winrt::make<DebugShapeInferrer>();

    winrt::check_hresult(registry->RegisterOperatorKernel(
        &kernelDescription,
        factory.get(),
        shareInferrer.get()
    ));
}