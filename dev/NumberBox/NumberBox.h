﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "pch.h"
#include "common.h"
#include "NumberBox.g.h"
#include "NumberBox.properties.h"
#include "Windows.Globalization.NumberFormatting.h"
#include "NumberBoxParser.h"
#include <regex>

class NumberBox :
    public ReferenceTracker<NumberBox, winrt::implementation::NumberBoxT>,
    public NumberBoxProperties
{
    
public:

    enum ValidationState
    {
        Valid,
        Invalid,
        InvalidMax,
        InvalidMin,
        InvalidInput,
        InvalidDivide
    };

    enum BoundState
    {
        InBounds,
        OverMax,
        UnderMin
    };

    NumberBox();

    // IFrameworkElement
    void OnApplyTemplate();

    void OnHeaderPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnSpinButtonPlacementModePropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnFractionDigitsPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnIntegerDigitsPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnIsDecimalPointAlwaysDisplayedPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnIsZeroSignedPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnSignificantDigitsPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnRoundingAlgorithmPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnNumberRounderPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnSignificantDigitPrecisionPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnIncrementPrecisionPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnPlaceholderTextPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnValuePropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnTextPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);

    void OnAcceptsCalculationsPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnBasicValidationModePropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnHyperScrollEnabledPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnMaxValuePropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnMinMaxModePropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnMinValuePropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnStepFrequencyPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    bool m_hasError{ false };


private:

    void OnTextBoxLostFocus(winrt::IInspectable const& sender, winrt::RoutedEventArgs const& args);
    void ValidateInput();
    void UpdateTextToValue();
    void SetErrorState(ValidationState state);
    void SetSpinButtonVisualState();
    void OnSpinDownClick(winrt::IInspectable const& sender, winrt::RoutedEventArgs const& args);
    void OnSpinUpClick(winrt::IInspectable const& sender, winrt::RoutedEventArgs const& args);
    void OnNumberBoxKeyUp(winrt::IInspectable const& sender, winrt::KeyRoutedEventArgs const& args);
    int ComputePrecisionRounderSigDigits(double newVal);
    void OnScroll(winrt::IInspectable const& sender, winrt::PointerRoutedEventArgs const& args);
    void StepValue(bool sign);
    void UpdateFormatter();
    void UpdateRounder();
    void SetHeader();
    void SetPlaceHolderText();
    void EvaluateInput();
    bool IsFormulaic(const winrt::hstring& in);
    void NormalizeShorthandOperations();
    BoundState GetBoundState(double val);
    winrt::DecimalFormatter m_formatter{};
    winrt::IncrementNumberRounder m_iRounder{};
    winrt::SignificantDigitsNumberRounder m_sRounder{};
    winrt::DecimalFormatter m_stepPrecisionFormatter{};
    winrt::SignificantDigitsNumberRounder m_stepPrecisionRounder{};
    winrt::TextBox m_TextBox{};
    winrt::RepeatButton m_SpinDown{};
    winrt::RepeatButton m_SpinUp{};
    winrt::FontIcon m_WarningIcon{};
    winrt::ToolTip m_ErrorToolTip{};
    winrt::TextBlock m_ErrorToolTipTextBlock{};
    winrt::TextBlock m_ErrorTextMessage{};
    winrt::hstring m_ValidationMessage{};

};
