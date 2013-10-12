/*
Copyright (c) 2013 Aerys

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "Container.hpp"

#include "minko/data/ArrayProvider.hpp"
#include "minko/data/Provider.hpp"

using namespace minko;
using namespace minko::data;

Container::Container() :
	std::enable_shared_from_this<Container>(),
	_providers(),
	_propertyNameToProvider(),
	_arrayLengths(data::Provider::create()),
	_propertyAdded(Container::PropertyChangedSignal::create()),
	_propertyRemoved(Container::PropertyChangedSignal::create()),
	_propValueChanged(),
	_propReferenceChanged(),
	_propertyAddedOrRemovedSlots(),
	_providerValueChangedSlot(),
	_providerReferenceChangedSlot()
{
}

void
Container::initialize()
{
	addProvider(_arrayLengths);
}

void
Container::addProvider(Provider::Ptr provider)
{
	assertProviderDoesNotExist(provider);

	_providers.push_back(provider);

	_propertyAddedOrRemovedSlots[provider].push_back(provider->propertyAdded()->connect(std::bind(
		&Container::providerPropertyAddedHandler,
		shared_from_this(),
		std::placeholders::_1,
		std::placeholders::_2
	)));
	
	_propertyAddedOrRemovedSlots[provider].push_back(provider->propertyRemoved()->connect(std::bind(
		&Container::providerPropertyRemovedHandler,
		shared_from_this(),
		std::placeholders::_1,
		std::placeholders::_2
	)));

	_providerReferenceChangedSlot[provider] = provider->propertyReferenceChanged()->connect(std::bind(
		&Container::providerReferenceChangedHandler,
		shared_from_this(),
		std::placeholders::_1,
		std::placeholders::_2
	));

	for (auto property : provider->values())
		providerPropertyAddedHandler(provider, property.first);
}

void
Container::removeProvider(Provider::Ptr provider)
{
	assertProviderExists(provider);

	for (auto property : provider->values())
		providerPropertyRemovedHandler(provider, property.first);
	
	_propertyAddedOrRemovedSlots.erase(provider);
	_providerValueChangedSlot.erase(provider);
	_providerReferenceChangedSlot.erase(provider);

	_providers.erase(std::find(_providers.begin(), _providers.end(), provider));

	/*
	for (auto property : provider->values())
		_propertyNameToProvider.erase(property.first);

	if (_providerValueChangedSlot.count(provider) != 0)
		_providerValueChangedSlot.erase(provider);

	_providerValueChangedSlot.erase(provider);
	*/
}

void
Container::addProvider(std::shared_ptr<ArrayProvider> provider)
{
	assertProviderDoesNotExist(provider);

	// Warning: the instruction order is very important here.

	const auto	lengthPropertyName	= provider->arrayName() + ".length";
	int			length				= _arrayLengths->hasProperty(lengthPropertyName)
		? _arrayLengths->get<int>(lengthPropertyName)
		: 0;

	provider->index(length);
	addProvider(std::dynamic_pointer_cast<Provider>(provider));

	_arrayLengths->set<int>(lengthPropertyName, ++length);
	// must come last because will trigger a program signature update which must 

}

void
Container::removeProvider(std::shared_ptr<ArrayProvider> provider)
{
	assertProviderExists(provider);

	auto	lengthPropertyName	= provider->arrayName() + ".length";
	int		length				= _arrayLengths->hasProperty(lengthPropertyName)
		? _arrayLengths->get<int>(lengthPropertyName)
		: 0;

	if (provider->index() != length-1)
	{
		auto last = std::dynamic_pointer_cast<ArrayProvider>(*std::find_if(
			_providers.rbegin(),
			_providers.rend(),
			[&](Provider::Ptr p)
			{
				auto arrayProvider = std::dynamic_pointer_cast<ArrayProvider>(p);

				return arrayProvider && arrayProvider->arrayName() == provider->arrayName();	
			}
		));

		last->index(provider->index());
	}

	removeProvider(std::dynamic_pointer_cast<Provider>(provider));

	_arrayLengths->set<int>(lengthPropertyName, --length);
	if (length == 0)
		_arrayLengths->unset(lengthPropertyName);
}

bool
Container::hasProvider(std::shared_ptr<Provider> provider)
{
	return std::find(_providers.begin(), _providers.end(), provider) != _providers.end();
}

bool
Container::hasProperty(const std::string& propertyName) const
{
	return _propertyNameToProvider.count(propertyName) != 0;
}

Container::PropertyChangedSignalPtr
Container::propertyValueChanged(const std::string& propertyName)
{
	//assertPropertyExists(propertyName);

	if (_propValueChanged.count(propertyName) == 0)
	{
		_propValueChanged[propertyName] = Signal<Container::Ptr, const std::string&>::create();

		if (_propertyNameToProvider.count(propertyName) != 0)
		{
			Provider::Ptr provider = _propertyNameToProvider[propertyName];

			if (_providerValueChangedSlot.count(provider) == 0)
				_providerValueChangedSlot[provider] = provider->propertyValueChanged()->connect(std::bind(
					&Container::providerValueChangedHandler,
					shared_from_this(),
					std::placeholders::_1,
					std::placeholders::_2
				));
		}
	}

	return _propValueChanged[propertyName];
}

Container::PropertyChangedSignalPtr
Container::propertyReferenceChanged(const std::string& propertyName)
{
	if (_propReferenceChanged.count(propertyName) == 0)
	{
		_propReferenceChanged[propertyName] = Signal<Container::Ptr, const std::string&>::create();

		if (_propertyNameToProvider.count(propertyName) != 0)
		{
			Provider::Ptr provider = _propertyNameToProvider[propertyName];

			if (_providerReferenceChangedSlot.count(provider) == 0)
			{
				_providerReferenceChangedSlot[provider] = provider->propertyReferenceChanged()->connect(std::bind(
					&Container::providerReferenceChangedHandler,
					shared_from_this(),
					std::placeholders::_1,
					std::placeholders::_2
				));
			}
		}
	}

	return _propReferenceChanged[propertyName];
}

void
Container::assertPropertyExists(const std::string& propertyName)
{
	if (!hasProperty(propertyName))
		throw std::invalid_argument(propertyName);	
}

void
Container::providerValueChangedHandler(Provider::Ptr			provider,
									      const std::string& 	propertyName)
{
	if (_propValueChanged.count(propertyName) != 0)
		propertyValueChanged(propertyName)->execute(shared_from_this(), propertyName);
}

void
Container::providerReferenceChangedHandler(Provider::Ptr		provider,
										   const std::string&	propertyName)
{
	if (_propReferenceChanged.count(propertyName))
		propertyReferenceChanged(propertyName)->execute(shared_from_this(), propertyName);
}

void
Container::providerPropertyAddedHandler(std::shared_ptr<Provider> 	provider,
										const std::string& 			propertyName)
{
	if (_propertyNameToProvider.count(propertyName) != 0)
		throw std::logic_error("duplicate property name: " + propertyName);
	
	_propertyNameToProvider[propertyName] = provider;

	if (_propValueChanged.count(propertyName) != 0)
		_providerValueChangedSlot[provider] = provider->propertyValueChanged()->connect(std::bind(
			&Container::providerValueChangedHandler,
			shared_from_this(),
			std::placeholders::_1,
			std::placeholders::_2
		));

	_propertyAdded->execute(shared_from_this(), propertyName);

	providerValueChangedHandler(provider, propertyName);
}

void
Container::providerPropertyRemovedHandler(std::shared_ptr<Provider> provider,
										  const std::string&		propertyName)
{
	_propertyNameToProvider.erase(propertyName);

	if (_propValueChanged.count(propertyName) && _propValueChanged[propertyName]->numCallbacks() == 0)
		_propValueChanged.erase(propertyName);

	if (_propReferenceChanged.count(propertyName) && _propReferenceChanged[propertyName]->numCallbacks() == 0)
		_propReferenceChanged.erase(propertyName);

	//_propValueChanged.erase(propertyName);
	//_propReferenceChanged.erase(propertyName);

	/*
	if (_providerValueChangedSlot.count(provider) != 0)
		for (auto property : provider->values())
			if (_propValueChanged.count(property.first) != 0)
				return;

	providerValueChangedHandler(provider, propertyName);

	std::cout << "cont[" << this << "] removes '" << propertyName << "'" << std::endl;
	*/

	_propertyRemoved->execute(shared_from_this(), propertyName);
}
