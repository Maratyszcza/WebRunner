from __future__ import absolute_import
import xml.etree.ElementTree as ET


class Kernel:
    def __init__(self, name, namespace=None):
        self.name = name
        self.namespace = namespace
        self.parameters = []
        self.arguments = []

    @property
    def full_name(self):
        if self.namespace is not None:
            return self.namespace + "::" + self.name
        else:
            return self.name

    @property
    def prefix(self):
        if self.namespace is not None:
            return self.namespace + "_" + self.name
        else:
            return self.name

    @property
    def header(self):
        if self.namespace is not None:
            return "kernels/" + self.namespace + "/" + self.name + "-gen.h"
        else:
            return "kernels/" + self.name + "-gen.h"


class Parameter:
    def __init__(self, name, type, default):
        assert type in ["uint32", "uint64"]

        self.name = name
        self.type = type
        self.default = default
        self.min = None
        self.max = None

    @property
    def c_type(self):
        return {
            "uint32": "uint32_t",
            "uint64": "uint64_t"
        }[self.type]

    @property
    def c_default(self):
        return {
            "uint32": "UINT32_C(",
            "uint64": "UINT64_C("
        }[self.type] + self.default + ")"

    @property
    def c_min(self):
        if self.min is not None:
            return {
                "uint32": "UINT32_C(",
                "uint64": "UINT64_C("
            }[self.type] + self.min + ")"

    @property
    def c_max(self):
        if self.max is not None:
            return {
                "uint32": "UINT32_C(",
                "uint64": "UINT64_C("
            }[self.type] + self.max + ")"


class Argument:
    def __init__(self, name, c_type):
        assert c_type in ["size_t",
            "uint8_t", "uint16_t", "uint32_t", "uint64_t",
            "int8_t", "int16_t", "int32_t", "int64_t",
            "const float*", "float*",
            "const double*", "double*"]

        self.name = name
        self.c_type = c_type


def read_kernel_specification(xml_filename):
    """Reads parameters specification from XML file a returns a :class:`Kernel` object
    :param xml_filename: path to an XML file with parameters specification
    """
    xml_tree = ET.parse(xml_filename)
    xml_kernel = xml_tree.getroot()
    assert xml_kernel.tag == "kernel"

    kernel = Kernel(xml_kernel.attrib["name"], xml_kernel.attrib.get("namespace"))

    for xml_element in xml_kernel:
        assert xml_element.tag in ["query", "call"]
        if xml_element.tag == "query":
            for xml_parameter in xml_element:
                assert xml_parameter.tag == "parameter"
                parameter = Parameter(xml_parameter.attrib["name"], xml_parameter.attrib["type"], xml_parameter.attrib["default"])
                parameter.min = xml_parameter.get("min")
                parameter.max = xml_parameter.get("max")
                kernel.parameters.append(parameter)
        elif xml_element.tag == "call":
            for xml_argument in xml_element:
                assert xml_argument.tag == "argument"
                argument = Argument(xml_argument.attrib["name"], xml_argument.attrib["type"])
                kernel.arguments.append(argument)
    return kernel
