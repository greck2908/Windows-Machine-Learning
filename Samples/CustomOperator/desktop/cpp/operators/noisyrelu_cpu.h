#pragma once

#include "MLOperatorAuthor.h"

struct NoisyReluShapeInferrer : winrt::implements<NoisyReluShapeInferrer, IMLOperatorShapeInferrer>
{
    STDMETHOD(InferOutputShapes)(IMLOperatorShapeInferenceContext* context) noexcept
    {
        try
        {
            uint32_t inputDimsSize;
            winrt::check_hresult(context->GetInputTensorDimensionCount(0, &inputDimsSize));

            std::vector<uint32_t> inputDims(inputDimsSize);
            winrt::check_hresult(context->GetInputTensorShape(0, inputDimsSize, inputDims.data()));

            winrt::check_hresult(context->SetOutputTensorShape(0, inputDimsSize, inputDims.data()));
            return S_OK;
        }
        catch (...)
        {
            return winrt::to_hresult();
        }
    }
};

struct NoisyReluOperator: winrt::implements<NoisyReluOperator, IMLOperatorKernel>
{
    float m_mean;
    float m_variance;

    NoisyReluOperator(float mean, float variance) :
        m_mean(mean),
        m_variance(variance)
    {}

    // Computes the outputs of the kernel.  This may be called multiple times
    // simultaneously within the same instance of the class.  Implementations
    // of this method must be thread-safe.
    STDMETHOD(Compute)(IMLOperatorKernelContext* context)
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
            winrt::check_hresult(outputTensor->GetShape(inputDimsSize, inputDims.data()));

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

            // If the tensor types are both float type
            if (outputTensor->GetTensorDataType() == MLOperatorTensorDataType::Float &&
                inputTensor->GetTensorDataType() == MLOperatorTensorDataType::Float)
            {
                // For cpu data
                if (outputTensor->IsCpuData() && inputTensor->IsCpuData())
                {
                    ComputeInternal<float>(inputTensor.get(), outputTensor.get(), inputDataSize);
                }
            }
            else if (outputTensor->GetTensorDataType() == MLOperatorTensorDataType::Double &&
                inputTensor->GetTensorDataType() == MLOperatorTensorDataType::Double)
            {
                // For cpu data
                if (outputTensor->IsCpuData() && inputTensor->IsCpuData())
                {
                    ComputeInternal<double>(inputTensor.get(), outputTensor.get(), inputDataSize);
                }
            }

            return S_OK;
        }
        catch (...)
        {
            return winrt::to_hresult();
        }
    }

    template <typename T, typename U = T>
    void ComputeInternal(IMLOperatorTensor* pInputTensor, IMLOperatorTensor* pOutputTensor, uint32_t size)
    {
        // Create a normal distribution
        std::normal_distribution<> dist{ m_mean, m_variance };
        std::random_device rd{};
        std::mt19937 gen{ rd() };

        auto inputData = static_cast<T*>(pInputTensor->GetData());
        auto outputData = static_cast<U*>(pOutputTensor->GetData());

        for (uint32_t i = 0; i < size; i++)
        {
            outputData[i] = static_cast<U>(std::max<T>(0, static_cast<T>(inputData[i] + dist(gen))));
        }
    }
};

struct NoisyReluOperatorFactory : winrt::implements<NoisyReluOperatorFactory, IMLOperatorKernelFactory>
{
    STDMETHOD(CreateKernel)(
        IMLOperatorKernelCreationContext* context,
        IMLOperatorKernel** kernel)
    {
        try
        {
            float mean;
            winrt::check_hresult(context->GetAttribute("mean", MLOperatorAttributeType::Float, 1, sizeof(float), reinterpret_cast<void*>(&mean)));
            float variance;
            winrt::check_hresult(context->GetAttribute("variance", MLOperatorAttributeType::Float, 1, sizeof(float), reinterpret_cast<void*>(&variance)));

            auto noisyReluOperator = winrt::make<NoisyReluOperator>(mean, variance);
            noisyReluOperator.copy_to(kernel);
            return S_OK;
        }
        catch (...)
        {
            return winrt::to_hresult();
        }
    }

    static MLOperatorEdgeDescription CreateEdgeDescriptor(
        MLOperatorEdgeType /*type*/,
        MLOperatorTensorDataType dataType)
    {
        MLOperatorEdgeDescription desc;
        desc.edgeType = MLOperatorEdgeType::Tensor;
        desc.tensorDataType = dataType;
        return desc;
    }

    static void RegisterNoisyReluSchema(winrt::com_ptr<IMLOperatorRegistry> registry)
    {
        MLOperatorSetId operatorSetId;
        operatorSetId.domain = "MyNoisyReluDomain";
        operatorSetId.version = 1;

        MLOperatorSchemaDescription noisyReluSchema = {};
        noisyReluSchema.name = "NoisyRelu";
        noisyReluSchema.operatorSetVersionAtLastChange = 1;

        MLOperatorSchemaEdgeDescription noisyReluXInput;
        noisyReluXInput.options = MLOperatorParameterOptions::Single;
        noisyReluXInput.typeFormat = MLOperatorSchemaEdgeTypeFormat::Label;
        noisyReluXInput.typeLabel = "T";

        std::vector<MLOperatorSchemaEdgeDescription> inputs { noisyReluXInput };
        noisyReluSchema.inputs = inputs.data();
        noisyReluSchema.inputCount = static_cast<uint32_t>(inputs.size());

        MLOperatorSchemaEdgeDescription noisyReluXOutput;
        noisyReluXOutput.options = MLOperatorParameterOptions::Single;
        noisyReluXOutput.typeFormat = MLOperatorSchemaEdgeTypeFormat::Label;
        noisyReluXOutput.typeLabel = "T";

        std::vector<MLOperatorSchemaEdgeDescription> outputs{ noisyReluXOutput };
        noisyReluSchema.outputs = outputs.data();
        noisyReluSchema.outputCount = static_cast<uint32_t>(outputs.size());

        MLOperatorEdgeTypeConstrant typeConstraint;
        typeConstraint.typeLabel = "T";
        std::vector<MLOperatorEdgeDescription> allowedEdges
        {
            CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Double),
            CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Float),
            CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Float16)
        };
        typeConstraint.allowedTypes = allowedEdges.data();
        typeConstraint.allowedTypeCount = static_cast<uint32_t>(allowedEdges.size());

        std::vector<MLOperatorEdgeTypeConstrant> typeConstraints { typeConstraint };
        noisyReluSchema.typeConstraints = typeConstraints.data();
        noisyReluSchema.typeConstraintCount = static_cast<uint32_t>(typeConstraints.size());

        MLOperatorAttribute noisyReluMeanAttribute;
        noisyReluMeanAttribute.name = "mean";
        noisyReluMeanAttribute.required = false;
        noisyReluMeanAttribute.type = MLOperatorAttributeType::Float;

        MLOperatorAttribute noisyReluVarianceAttribute;
        noisyReluVarianceAttribute.name = "variance";
        noisyReluVarianceAttribute.required = false;
        noisyReluVarianceAttribute.type = MLOperatorAttributeType::Float;

        std::vector<MLOperatorAttribute> attributes { noisyReluMeanAttribute, noisyReluVarianceAttribute };
        noisyReluSchema.attributes = attributes.data();
        noisyReluSchema.attributeCount = static_cast<uint32_t>(attributes.size());

        MLOperatorAttributeNameValue noisyReluMeanAttributeValue;
        noisyReluMeanAttributeValue.name = "mean";
        noisyReluMeanAttributeValue.type = MLOperatorAttributeType::Float;
        noisyReluMeanAttributeValue.valueCount = 1;
        static float defaultMeans[] = { 0.f };
        noisyReluMeanAttributeValue.floats = defaultMeans;

        MLOperatorAttributeNameValue noisyReluVarianceAttributeValue;
        noisyReluVarianceAttributeValue.name = "variance";
        noisyReluVarianceAttributeValue.type = MLOperatorAttributeType::Float;
        noisyReluVarianceAttributeValue.valueCount = 1;
        static float defaultVariance[] = { 1.f };
        noisyReluVarianceAttributeValue.floats = defaultVariance;

        std::vector<MLOperatorAttributeNameValue> attributeDefaultValues { noisyReluMeanAttributeValue, noisyReluVarianceAttributeValue };
        noisyReluSchema.defaultAttributes = attributeDefaultValues.data();
        noisyReluSchema.defaultAttributeCount = static_cast<uint32_t>(attributeDefaultValues.size());

        std::vector<const MLOperatorSchemaDescription*> schemas { &noisyReluSchema };
        winrt::check_hresult(registry->RegisterOperatorSetSchema(
            &operatorSetId,
            7 /* baseline version */,
            schemas.data(),
            static_cast<uint32_t>(schemas.size()),
            nullptr,
            nullptr
        ));
    }

    static void RegisterNoisyReluKernel(winrt::com_ptr<IMLOperatorRegistry> registry)
    {
        MLOperatorKernelDescription kernelDescription = {};
        kernelDescription.domain = "MyNoisyReluDomain";
        kernelDescription.name = "NoisyRelu";
        kernelDescription.minimumOperatorSetVersion = 1;
        kernelDescription.executionType = MLOperatorExecutionType::Cpu;

        MLOperatorEdgeTypeConstrant typeConstraint;
        typeConstraint.typeLabel = "T";
        std::vector<MLOperatorEdgeDescription> allowedEdges
        {
            CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Double),
            CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Float),
            CreateEdgeDescriptor(MLOperatorEdgeType::Tensor, MLOperatorTensorDataType::Float16)
        };
        typeConstraint.allowedTypes = allowedEdges.data();
        typeConstraint.allowedTypeCount = static_cast<uint32_t>(allowedEdges.size());

        std::vector<MLOperatorEdgeTypeConstrant> typeConstraints{ typeConstraint };
        kernelDescription.typeConstraints = typeConstraints.data();
        kernelDescription.typeConstraintCount = static_cast<uint32_t>(typeConstraints.size());

        MLOperatorAttributeNameValue noisyReluMeanAttributeValue;
        noisyReluMeanAttributeValue.name = "mean";
        noisyReluMeanAttributeValue.type = MLOperatorAttributeType::Float;
        noisyReluMeanAttributeValue.valueCount = 1;
        static float defaultMeans[] = { 0.f };
        noisyReluMeanAttributeValue.floats = defaultMeans;

        MLOperatorAttributeNameValue noisyReluVarianceAttributeValue;
        noisyReluVarianceAttributeValue.name = "variance";
        noisyReluVarianceAttributeValue.type = MLOperatorAttributeType::Float;
        noisyReluVarianceAttributeValue.valueCount = 1;
        static float defaultVariance[] = { 1.f };
        noisyReluVarianceAttributeValue.floats = defaultVariance;

        std::vector<MLOperatorAttributeNameValue> attributeDefaultValues{ noisyReluMeanAttributeValue, noisyReluVarianceAttributeValue };
        kernelDescription.defaultAttributes = attributeDefaultValues.data();
        kernelDescription.defaultAttributeCount = static_cast<uint32_t>(attributeDefaultValues.size());
        kernelDescription.options = MLOperatorKernelOptions::None;
        kernelDescription.executionOptions = 0;

        auto factory = winrt::make<NoisyReluOperatorFactory>();
        auto shareInferrer = winrt::make<NoisyReluShapeInferrer>();

        winrt::check_hresult(registry->RegisterOperatorKernel(
            &kernelDescription,
            factory.get(),
            shareInferrer.get()
        ));
    }
};
