// Copyright Max Harris

#include "PCGUtilsPCGExInteropEditor.h"

#include "Collections/PCGUtilsCollectionCategoryHandle.h"
#include "Customizations/PCGUtilsCollectionCategoryHandleCustomization.h"
#include "PropertyEditorModule.h"

void FPCGUtilsPCGExInteropEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditor.RegisterCustomPropertyTypeLayout(
		FPCGUtilsCollectionCategoryHandle::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGUtilsCollectionCategoryHandleCustomization::MakeInstance));
	PropertyEditor.NotifyCustomizationModuleChanged();
}

void FPCGUtilsPCGExInteropEditorModule::ShutdownModule()
{
	if (FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyEditor->UnregisterCustomPropertyTypeLayout(FPCGUtilsCollectionCategoryHandle::StaticStruct()->GetFName());
		PropertyEditor->NotifyCustomizationModuleChanged();
	}
}

IMPLEMENT_MODULE(FPCGUtilsPCGExInteropEditorModule, PCGUtilsPCGExInteropEditor)
