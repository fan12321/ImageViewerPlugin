#include "ImageSettingsAction.h"
#include "Layer.h"

#include <util/ColorSpace.h>
#include <util/Interpolation.h>
#include <event/Event.h>

#include <PointData/PointData.h>
#include <ClusterData/ClusterData.h>

using namespace mv;
using namespace mv::util;

ImageSettingsAction::ImageSettingsAction(QObject* parent, const QString& title) :
    GroupAction(parent, title, true),
    _layer(nullptr),
    _opacityAction(this, "Opacity", 0.0f, 100.0f, 100.0f, 1),
    _subsampleFactorAction(this, "Subsample", 1, 8, 1),
    _colorSpaceAction(this, "Color space", colorSpaces.values(), "Mono"),
    _scalarChannel1Action(this, ScalarChannelAction::channelIndexes.value(ScalarChannelAction::Channel1)),
    _scalarChannel2Action(this, ScalarChannelAction::channelIndexes.value(ScalarChannelAction::Channel2)),
    _scalarChannel3Action(this, ScalarChannelAction::channelIndexes.value(ScalarChannelAction::Channel3)),
    _colorMap1DAction(this, "1D Color map", "Black to white"),
    _colorMap2DAction(this, "2D Color map", "example_a"),
    _interpolationTypeAction(this, "Interpolate", interpolationTypes.values(), "Bilinear"),
    _useConstantColorAction(this, "Use constant color", false),
    _fixChannelRangesToColorSpaceAction(this, "Set channel ranges to color space", false),
    _constantColorAction(this, "Constant color", QColor(Qt::white)), 
    _useBackgroundAction(this, "Background"), 
    _backgroundDimAction(this, "Background Dimension")
{
    addAction(&_opacityAction);
    addAction(&_subsampleFactorAction);
    addAction(&_colorSpaceAction);
    addAction(&_useBackgroundAction);
    addAction(&_backgroundDimAction);
    addAction(&_scalarChannel1Action);
    addAction(&_scalarChannel2Action);
    addAction(&_scalarChannel3Action);
    addAction(&_colorMap1DAction);
    addAction(&_colorMap2DAction);
    addAction(&_interpolationTypeAction);
    addAction(&_useConstantColorAction);
    addAction(&_fixChannelRangesToColorSpaceAction);
    addAction(&_constantColorAction);

    _subsampleFactorAction.setVisible(false);

    _useBackgroundAction.setVisible(false);
    _backgroundDimAction.setVisible(false);

    _opacityAction.setToolTip("Image layer opacity");
    _subsampleFactorAction.setToolTip("Subsampling factor");
    _scalarChannel1Action.setToolTip("Scalar channel 1");
    _scalarChannel2Action.setToolTip("Scalar channel 2");
    _scalarChannel3Action.setToolTip("Scalar channel 3");
    _colorSpaceAction.setToolTip("The color space used to shade the image");
    _colorMap1DAction.setToolTip("Image one-dimensional color map");
    _colorMap2DAction.setToolTip("Image two-dimensional color map");
    _interpolationTypeAction.setToolTip("The type of two-dimensional image interpolation used");
    _useConstantColorAction.setToolTip("Use constant color to shade the image");
    _fixChannelRangesToColorSpaceAction.setToolTip("In this mode, data ranges are ignored and the channel ranges are set to the current color space range (RGB, HSL or LAB)");
    _constantColorAction.setToolTip("Constant color");

    _opacityAction.setSuffix("%");

    _colorMap1DAction.getRangeAction(ColorMapAction::Axis::X).setEnabled(false);
    _colorMap1DAction.getRangeAction(ColorMapAction::Axis::Y).setEnabled(false);

    const auto useConstantColorToggled = [this]() {
        _colorMap1DAction.setEnabled(!_useConstantColorAction.isChecked() && (_colorSpaceAction.getCurrentIndex() == 0));
        _colorMap2DAction.setEnabled(!_useConstantColorAction.isChecked() && (_colorSpaceAction.getCurrentIndex() == 1));
        _constantColorAction.setEnabled(_useConstantColorAction.isChecked());
    };

    useConstantColorToggled();

    connect(&_useConstantColorAction, &ToggleAction::toggled, this, useConstantColorToggled);

    connect(&_fixChannelRangesToColorSpaceAction, &ToggleAction::toggled, this, [this](bool toggled) {

        // Set color space range
        if (_fixChannelRangesToColorSpaceAction.isChecked()) {

            // only set color space range for RGB, HSL and LAB
            switch (static_cast<ColorSpaceType>(_colorSpaceAction.getCurrentIndex())) {
	            case ColorSpaceType::RGB: {
	                _scalarChannel1Action.setColorSpaceRange(true, 0, 255); // red
	                _scalarChannel2Action.setColorSpaceRange(true, 0, 255); // green
	                _scalarChannel3Action.setColorSpaceRange(true, 0, 255); // blue
	                break;
	            }

	            case ColorSpaceType::HSL: {
	                _scalarChannel1Action.setColorSpaceRange(true, 0, 360); // hue
	                _scalarChannel2Action.setColorSpaceRange(true, 0, 1);   // saturation
	                _scalarChannel3Action.setColorSpaceRange(true, 0, 1);   // value
	                break;
	            }

	            case ColorSpaceType::LAB: {
	                _scalarChannel1Action.setColorSpaceRange(true, 0, 100);     // L
	                _scalarChannel2Action.setColorSpaceRange(true, -128, 127);  // A
	                _scalarChannel3Action.setColorSpaceRange(true, -128, 127);  // B
	                break;
	            }

	            default: {
	                _scalarChannel1Action.setColorSpaceRange(false);
	                _scalarChannel2Action.setColorSpaceRange(false);
	                _scalarChannel3Action.setColorSpaceRange(false);
	                break;
	            }

            } // switch
        }
        else
        {
            _scalarChannel1Action.setColorSpaceRange(false);
            _scalarChannel2Action.setColorSpaceRange(false);
            _scalarChannel3Action.setColorSpaceRange(false);
        }

        // update channels
        emit channelChanged(_scalarChannel1Action);
        emit channelChanged(_scalarChannel2Action);
        emit channelChanged(_scalarChannel3Action);

    });

    connect(&_scalarChannel1Action, &ScalarChannelAction::changed, this, [this]() {
        emit channelChanged(_scalarChannel1Action);
    });
    
    connect(&_scalarChannel2Action, &ScalarChannelAction::changed, this, [this]() {
        emit channelChanged(_scalarChannel2Action);
    });

    connect(&_scalarChannel3Action, &ScalarChannelAction::changed, this, [this]() {
        emit channelChanged(_scalarChannel3Action);
    });
}

void ImageSettingsAction::initialize(Layer* layer)
{
    Q_ASSERT(layer != nullptr);

    if (layer == nullptr)
        return;

    _layer = layer;

    _scalarChannel1Action.initialize(_layer, ScalarChannelAction::Channel1);
    _scalarChannel2Action.initialize(_layer, ScalarChannelAction::Channel2);
    _scalarChannel3Action.initialize(_layer, ScalarChannelAction::Channel3);

    updateColorMapImage();

    const auto isClusterType    = _layer->getSourceDataset()->getDataType() == ClusterType;
    const auto dimensionNames   = _layer->getDimensionNames();

    connect(&_useConstantColorAction, &ToggleAction::toggled, _layer, &Layer::invalidate);
    connect(&_opacityAction, &DecimalAction::valueChanged, _layer, &Layer::invalidate);
    connect(&_subsampleFactorAction, &IntegralAction::valueChanged, _layer, &Layer::invalidate);
    connect(&_interpolationTypeAction, &OptionAction::currentIndexChanged, _layer, &Layer::invalidate);
    connect(&_constantColorAction, &ColorAction::colorChanged, _layer, &Layer::invalidate);

    //connect(&_colorSpaceAction, &OptionAction::currentIndexChanged, _layer, &Layer::invalidate);
    //connect(&_colorSpaceAction, &OptionAction::currentIndexChanged, this, &ImageSettingsAction::updateColorMapImage);

    if (dimensionNames.count() == 1)
        _colorSpaceAction.setCurrentText("Mono");

    if (dimensionNames.count() == 2)
        _colorSpaceAction.setCurrentText("Duo");

    _scalarChannel1Action.getDimensionAction().setOptions(dimensionNames);
    _scalarChannel2Action.getDimensionAction().setOptions(dimensionNames);
    _scalarChannel3Action.getDimensionAction().setOptions(dimensionNames);

    _scalarChannel1Action.getDimensionAction().setCurrentIndex(0);

    if (isClusterType) {
        _interpolationTypeAction.setCurrentIndex(static_cast<std::int32_t>(InterpolationType::NearestNeighbor));
        _interpolationTypeAction.setEnabled(false);

        _colorSpaceAction.setEnabled(false);
        _colorSpaceAction.setCurrentText("Mono");

        _colorMap1DAction.setEnabled(false);
        _colorMap2DAction.setEnabled(false);

        _useConstantColorAction.setEnabled(false);

        _scalarChannel1Action.getWindowLevelAction().setEnabled(false);
        _scalarChannel2Action.getWindowLevelAction().setEnabled(false);
        _scalarChannel3Action.getWindowLevelAction().setEnabled(false);

        _useBackgroundAction.setVisible(true);
        _useBackgroundAction.setChecked(false);
        _backgroundDimAction.setVisible(true);
        _backgroundDimAction.setEnabled(false);
        connect(&_useBackgroundAction, &ToggleAction::toggled, this, [this]() { _backgroundDimAction.setEnabled(_useBackgroundAction.isChecked()); });
        // get parent point dataset's dimensions
        auto parent = Dataset<Clusters>(_layer->getSourceDataset());
        while (parent->getDataType() != PointType) {
            parent = parent->getParent();
        }
        Dataset<Points> points = parent;
        if (points.isValid()) {
            QStringList parentDimNames;
            for (auto name: points->getDimensionNames()) parentDimNames << name;

            _backgroundDimAction.setOptions(parentDimNames);
            _backgroundDimAction.setCurrentIndex(0);
        }

    }
    else {
        if (_layer->getNumberOfImages() >= 2) {
            _scalarChannel2Action.getDimensionAction().setCurrentIndex(1);
        }

        if (_layer->getNumberOfImages() >= 3) {
            _scalarChannel3Action.getDimensionAction().setCurrentIndex(2);
        }
    }

    connect(&_backgroundDimAction, &OptionAction::currentIndexChanged, this, &ImageSettingsAction::updateColorMapImage);
    connect(&_useBackgroundAction, &ToggleAction::toggled, this, &ImageSettingsAction::updateColorMapImage);

    connect(&_colorMap1DAction, &ColorMapAction::imageChanged, this, &ImageSettingsAction::updateColorMapImage);
    connect(&_colorMap2DAction, &ColorMapAction::imageChanged, this, &ImageSettingsAction::updateColorMapImage);
    //connect(&_colorMapAction.getDiscretizeAction(), &ToggleAction::toggled, this, &ImageSettingsAction::updateColorMapImage);

    const auto updateScalarChannels = [this]() {
        _scalarChannel1Action.computeScalarData();
        _scalarChannel2Action.computeScalarData();
        _scalarChannel3Action.computeScalarData();
    };

    _updateSelectionTimer.setSingleShot(true);
    _updateScalarDataTimer.setSingleShot(true);

    connect(&_updateSelectionTimer, &QTimer::timeout, this, [this]() -> void {
        if (_updateSelectionTimer.isActive())
            _updateSelectionTimer.start(LAZY_UPDATE_INTERVAL);
        else {
            _updateSelectionTimer.stop();
            _layer->computeSelectionIndices();
        }
    });

    connect(&_updateScalarDataTimer, &QTimer::timeout, this, [this, updateScalarChannels]() -> void {
        if (_updateScalarDataTimer.isActive())
            _updateScalarDataTimer.start(LAZY_UPDATE_INTERVAL);
        else {
            _updateScalarDataTimer.stop();
            updateScalarChannels();
        }
        });

    auto& sourceDataset = _layer->getSourceDataset();
    
    if (sourceDataset.isValid()) {
        connect(&sourceDataset, &Dataset<DatasetImpl>::dataChanged, [this, sourceDataset]() -> void {
            _updateScalarDataTimer.start(LAZY_UPDATE_INTERVAL);

            if (sourceDataset->getDataType() == ClusterType)
                updateColorMapImage();
        });

        connect(&sourceDataset, &Dataset<DatasetImpl>::dataSelectionChanged, [this]() -> void {
            _updateSelectionTimer.start(LAZY_UPDATE_INTERVAL);
        });
    }

    updateScalarChannels();

    connect(&_colorSpaceAction, &OptionAction::currentIndexChanged, this, &ImageSettingsAction::colorSpaceChanged);

    colorSpaceChanged();
}

const std::uint32_t ImageSettingsAction::getNumberOfActiveScalarChannels() const
{
    switch (static_cast<ColorSpaceType>(_colorSpaceAction.getCurrentIndex()))
    {
        case ColorSpaceType::Mono:
            return 1;

        case ColorSpaceType::Duo:
            return 2;

        case ColorSpaceType::RGB:
        case ColorSpaceType::HSL:
        case ColorSpaceType::LAB:
            return 3;

        default:
            break;
    }

    return 0;
}

QImage ImageSettingsAction::getColorMapImage()
{
    if (_layer->getSourceDataset()->getDataType() == ClusterType) {
        const auto& clusters = Dataset<Clusters>(_layer->getSourceDataset())->getClusters();

        // get parent point dataset's dimensions
        auto parent = Dataset<Clusters>(_layer->getSourceDataset());
        while (parent->getDataType() != PointType) {
            parent = parent->getParent();
        }
        Dataset<Points> points = parent;
        _backgroundMin = 0.0;
        _backgroundMax = 0.0;
        if (points.isValid() && _useBackgroundAction.isChecked()) {
            float min = std::numeric_limits<float>::max();
            float max = std::numeric_limits<float>::min();
            int dim = _backgroundDimAction.getCurrentIndex();
            for (const auto& cluster: clusters) {
                if (dim < 0) break;
                int numIndices = cluster.getNumberOfIndices();
                std::vector<float> data(numIndices);
                points->populateDataForDimensions(data, std::vector<int>{dim}, cluster.getIndices());
                min = std::min(*std::min_element(data.begin(), data.end()), min);
                max = std::max(*std::max_element(data.begin(), data.end()), max);
            }
            _backgroundMin = min;
            _backgroundMax = max;

            if (dim >= 0) {
                int numPixels = _layer->getImagesDataset()->getNumberOfPixels();
                std::vector<float> fullImage(numPixels);
                points->extractDataForDimension(fullImage, dim);
                
                _scalarData.clear();
                _scalarData.reserve(numPixels);
                std::copy(fullImage.begin(), fullImage.end(), std::back_inserter(_scalarData));
            }
        }
        _scalarDataRange.first = _backgroundMin;
        _scalarDataRange.second = _backgroundMax;

        QImage discreteColorMapImage(static_cast<std::int32_t>(clusters.size()), 1, QImage::Format::Format_RGB32);

        auto clusterIndex = 0;

        for (const auto& cluster : clusters) {
            discreteColorMapImage.setPixelColor(clusterIndex, 0, cluster.getColor());
            clusterIndex++;
        }

        return discreteColorMapImage.convertToFormat(QImage::Format_RGB32);
    }
    else {
        switch (_colorSpaceAction.getCurrentIndex())
        {
            case 0:
                return _colorMap1DAction.getColorMapImage();

            case 1:
                return _colorMap2DAction.getColorMapImage();

            default:
                break;
        }
    }

    return {};
}

void ImageSettingsAction::updateColorMapImage()
{
    auto interpolationType = InterpolationType::Bilinear;

    switch (_colorSpaceAction.getCurrentIndex())
    {
        case 0:
        case 1:
            interpolationType = _colorMap2DAction.getDiscretizeAction().isChecked() ? InterpolationType::NearestNeighbor : InterpolationType::Bilinear;
            break;

        default:
            break;
    }


    if (_colorSpaceAction.getCurrentIndex() <= 1) {
        _layer->setColorMapImage(getColorMapImage(), interpolationType);
        if (_layer->getSourceDataset()->getDataType() == ClusterType && _scalarData.size() > 0) {
            _layer->setChannelScalarData(ScalarChannelAction::Identifier::Channel2, _scalarData, _scalarDataRange);
        }
    }
}

void ImageSettingsAction::colorSpaceChanged()
{
    const auto isClusterType = _layer->getSourceDataset()->getDataType() == ClusterType;

    switch (static_cast<ColorSpaceType>(_colorSpaceAction.getCurrentIndex()))
    {
        case ColorSpaceType::Mono:
        {
            _scalarChannel1Action.getEnabledAction().setChecked(true);
            _scalarChannel2Action.getEnabledAction().setChecked(false);
            _scalarChannel3Action.getEnabledAction().setChecked(false);

            _scalarChannel1Action.setText("Channel 1");
            _scalarChannel2Action.setText("Channel 2");
            _scalarChannel3Action.setText("Channel 3");

            _colorMap1DAction.setEnabled(isClusterType ? false : (!_useConstantColorAction.isChecked()));
            _colorMap2DAction.setEnabled(false);

            _fixChannelRangesToColorSpaceAction.setChecked(false);
            _fixChannelRangesToColorSpaceAction.setEnabled(false);

            break;
        }

        case ColorSpaceType::Duo:
        {
            _scalarChannel1Action.getEnabledAction().setChecked(true);
            _scalarChannel2Action.getEnabledAction().setChecked(true);
            _scalarChannel3Action.getEnabledAction().setChecked(false);

            _scalarChannel1Action.setText("Channel 1");
            _scalarChannel2Action.setText("Channel 2");
            _scalarChannel3Action.setText("Channel 3");

            _colorMap1DAction.setEnabled(false);
            _colorMap2DAction.setEnabled(!_useConstantColorAction.isChecked());

            _fixChannelRangesToColorSpaceAction.setChecked(false);
            _fixChannelRangesToColorSpaceAction.setEnabled(false);

            break;
        }

        case ColorSpaceType::RGB:
        {
            _scalarChannel1Action.getEnabledAction().setChecked(true);
            _scalarChannel2Action.getEnabledAction().setChecked(true);
            _scalarChannel3Action.getEnabledAction().setChecked(true);

            _scalarChannel1Action.setText("Red");
            _scalarChannel2Action.setText("Green");
            _scalarChannel3Action.setText("Blue");

            _colorMap1DAction.setEnabled(false);
            _colorMap2DAction.setEnabled(false);

            _fixChannelRangesToColorSpaceAction.setEnabled(true);

            break;
        }

        case ColorSpaceType::HSL:
        {
            _scalarChannel1Action.getEnabledAction().setChecked(true);
            _scalarChannel2Action.getEnabledAction().setChecked(true);
            _scalarChannel3Action.getEnabledAction().setChecked(true);

            _scalarChannel1Action.setText("Hue");
            _scalarChannel2Action.setText("Saturation");
            _scalarChannel3Action.setText("Lightness");

            _colorMap1DAction.setEnabled(false);
            _colorMap2DAction.setEnabled(false);

            _fixChannelRangesToColorSpaceAction.setEnabled(true);

            break;
        }

        case ColorSpaceType::LAB:
        {
            _scalarChannel1Action.getEnabledAction().setChecked(true);
            _scalarChannel2Action.getEnabledAction().setChecked(true);
            _scalarChannel3Action.getEnabledAction().setChecked(true);

            _scalarChannel1Action.setText("L");
            _scalarChannel2Action.setText("A");
            _scalarChannel3Action.setText("B");

            _colorMap1DAction.setEnabled(false);
            _colorMap2DAction.setEnabled(false);

            _fixChannelRangesToColorSpaceAction.setEnabled(true);

            break;
        }

        default:
            break;
    }

    updateColorMapImage();

    _scalarChannel1Action.computeScalarData();
    _scalarChannel2Action.computeScalarData();
    _scalarChannel3Action.computeScalarData();

    _layer->invalidate();
}

void ImageSettingsAction::connectToPublicAction(WidgetAction* publicAction, bool recursive)
{
    auto publicImageSettingsAction = dynamic_cast<ImageSettingsAction*>(publicAction);

    Q_ASSERT(publicImageSettingsAction != nullptr);

    if (publicImageSettingsAction == nullptr)
        return;

    if (recursive) {
        actions().connectPrivateActionToPublicAction(&_opacityAction, &publicImageSettingsAction->getOpacityAction(), recursive);
        actions().connectPrivateActionToPublicAction(&_subsampleFactorAction, &publicImageSettingsAction->getSubsampleFactorAction(), recursive);
        actions().connectPrivateActionToPublicAction(&_colorSpaceAction, &publicImageSettingsAction->getColorSpaceAction(), recursive);
        actions().connectPrivateActionToPublicAction(&_scalarChannel1Action, &publicImageSettingsAction->getScalarChannel1Action(), recursive);
        actions().connectPrivateActionToPublicAction(&_scalarChannel2Action, &publicImageSettingsAction->getScalarChannel2Action(), recursive);
        actions().connectPrivateActionToPublicAction(&_scalarChannel3Action, &publicImageSettingsAction->getScalarChannel3Action(), recursive);
        actions().connectPrivateActionToPublicAction(&_colorMap1DAction, &publicImageSettingsAction->getColorMap1DAction(), recursive);
        actions().connectPrivateActionToPublicAction(&_colorMap2DAction, &publicImageSettingsAction->getColorMap2DAction(), recursive);
        actions().connectPrivateActionToPublicAction(&_interpolationTypeAction, &publicImageSettingsAction->getInterpolationTypeAction(), recursive);
        actions().connectPrivateActionToPublicAction(&_useConstantColorAction, &publicImageSettingsAction->getUseConstantColorAction(), recursive);
        actions().connectPrivateActionToPublicAction(&_constantColorAction, &publicImageSettingsAction->getConstantColorAction(), recursive);
    }

    GroupAction::connectToPublicAction(publicAction, recursive);
}

void ImageSettingsAction::disconnectFromPublicAction(bool recursive)
{
    if (!isConnected())
        return;

    if (recursive) {
        actions().disconnectPrivateActionFromPublicAction(&_opacityAction, recursive);
        actions().disconnectPrivateActionFromPublicAction(&_subsampleFactorAction, recursive);
        actions().disconnectPrivateActionFromPublicAction(&_colorSpaceAction, recursive);
        actions().disconnectPrivateActionFromPublicAction(&_scalarChannel1Action, recursive);
        actions().disconnectPrivateActionFromPublicAction(&_scalarChannel2Action, recursive);
        actions().disconnectPrivateActionFromPublicAction(&_scalarChannel3Action, recursive);
        actions().disconnectPrivateActionFromPublicAction(&_colorMap1DAction, recursive);
        actions().disconnectPrivateActionFromPublicAction(&_colorMap2DAction, recursive);
        actions().disconnectPrivateActionFromPublicAction(&_interpolationTypeAction, recursive);
        actions().disconnectPrivateActionFromPublicAction(&_useConstantColorAction, recursive);
        actions().disconnectPrivateActionFromPublicAction(&_constantColorAction, recursive);
    }

    GroupAction::disconnectFromPublicAction(recursive);
}

void ImageSettingsAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);

    _opacityAction.fromParentVariantMap(variantMap);
    _subsampleFactorAction.fromParentVariantMap(variantMap);
    _colorSpaceAction.fromParentVariantMap(variantMap);
    _scalarChannel1Action.fromParentVariantMap(variantMap);
    _scalarChannel2Action.fromParentVariantMap(variantMap);
    _scalarChannel3Action.fromParentVariantMap(variantMap);
    _colorMap1DAction.fromParentVariantMap(variantMap);
    _colorMap2DAction.fromParentVariantMap(variantMap);
    _interpolationTypeAction.fromParentVariantMap(variantMap);
    _useConstantColorAction.fromParentVariantMap(variantMap);
    _constantColorAction.fromParentVariantMap(variantMap);
}

QVariantMap ImageSettingsAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();

    _opacityAction.insertIntoVariantMap(variantMap);
    _subsampleFactorAction.insertIntoVariantMap(variantMap);
    _colorSpaceAction.insertIntoVariantMap(variantMap);
    _scalarChannel1Action.insertIntoVariantMap(variantMap);
    _scalarChannel2Action.insertIntoVariantMap(variantMap);
    _scalarChannel3Action.insertIntoVariantMap(variantMap);
    _colorMap1DAction.insertIntoVariantMap(variantMap);
    _colorMap2DAction.insertIntoVariantMap(variantMap);
    _interpolationTypeAction.insertIntoVariantMap(variantMap);
    _useConstantColorAction.insertIntoVariantMap(variantMap);
    _constantColorAction.insertIntoVariantMap(variantMap);

    return variantMap;
}