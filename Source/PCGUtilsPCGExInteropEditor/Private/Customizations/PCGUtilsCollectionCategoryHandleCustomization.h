// Copyright Max Harris

#pragma once

#include "IPropertyTypeCustomization.h"
#include "Widgets/Input/SComboBox.h"

class UPCGExAssetCollection;

/**
 * Details customization for FPCGUtilsCollectionCategoryHandle: the ordinary collection picker, and a Category
 * dropdown listing the categories the picked collection's entries use.
 */
class FPCGUtilsCollectionCategoryHandleCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> PropertyHandle,
		class FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> PropertyHandle,
		class IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	TSharedPtr<IPropertyHandle> StructHandle;
	TSharedPtr<IPropertyHandle> CollectionHandle;
	TSharedPtr<IPropertyHandle> CategoryHandle;

	TSharedPtr<SComboBox<TSharedPtr<FName>>> CategoryComboBox;
	TArray<TSharedPtr<FName>> CategoryOptions;

	/** The categories of the collection last read, so the current value is checked without walking entries per frame. */
	TSet<FName> KnownCategories;

	/** The collection every edited handle shares. Null when none is assigned, or the handles disagree. */
	UPCGExAssetCollection* GetSharedCollection(bool& bOutMultipleCollections) const;

	/** Re-reads the collection's categories into the dropdown options. */
	void RefreshCategories();

	void OnCategorySelected(TSharedPtr<FName> Selected, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> MakeCategoryOptionWidget(TSharedPtr<FName> Option) const;

	FText GetCategoryText() const;
	FText GetCategoryToolTip() const;
	FSlateColor GetCategoryColor() const;
	FText GetHeaderText() const;
	bool CanPickCategory() const;

	/** True when the shared category is set and the shared collection's entries do not use it. */
	bool IsCategoryMissing() const;
};
