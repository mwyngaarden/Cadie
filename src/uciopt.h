#ifndef UCIOPT_H
#define UCIOPT_H

#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <cassert>

class UCIOption {
public:
    enum OptionType {
        Check,
        Spin,
        Combo,
        Button,
        String
    };

    UCIOption(std::string name, OptionType otype) : name_(name), type_(otype) 
    {
    }

    UCIOption(std::string name, bool def) : UCIOption(name, Check)
    {
        check_def_ = def;
        check_value_ = def;
    }
    
    UCIOption(std::string name, int min, int def, int max) : UCIOption(name, Spin)
    {
        spin_min_ = min;
        spin_def_ = def;
        spin_max_ = max;
        spin_value_ = def;
    }
    
    UCIOption(std::string name, std::string def, std::vector<std::string> values) 
        : UCIOption(name, Combo)
    {
        combo_def_ = def;
        combo_values_ = values;
        combo_value_ = def;
    }

    UCIOption(std::string name) : UCIOption(name, Button)
    {
    }
    
    UCIOption(std::string name, std::string def) : UCIOption(name, String)
    {
        string_def_ = def;
        string_value_ = def;
    }

    std::string opt_to_string() const
    {
        switch (type_) {
        case Check:
            return check_value_ ? "true" : "false";
        case Spin:
            return std::to_string(spin_value_);
        case Combo:
            return combo_value_;
        case Button:
            assert(false);
        case String:
            return string_value_;
        default:
            assert(false);
            return std::string();
        }
    }

    std::string name() const { return name_; }

    static std::string to_string(OptionType type)
    {
        switch (type) {
        case Check:     return "check";
        case Spin:      return "spin";
        case Combo:     return "combo";
        case Button:    return "button";
        case String:    return "string";
        default:
            assert(false);
            return std::string();
        }
    }

    int clamp(int value) const
    {
        assert(type_ == Spin);

        if (value < spin_min_) 
            return spin_min_;
        else if (value > spin_max_) 
            return spin_max_;
        else
            return value;
    }

    std::string to_option() const
    {
        std::ostringstream oss;

        oss << "option name " << name_ << " type " << to_string(type_);

        if (type_ == Button) return oss.str();

        oss << " default ";

        switch (type_) {
        case Check:
            oss << (check_def_ ? "true" : "false");
            break;
        case Spin:
            oss << spin_def_ << " min " << spin_min_ << " max " << spin_max_;
            break;
        case Combo:
            {
            oss << combo_def_;

            for (std::string val : combo_values_)
                oss << " val " << val;
            }
            break;
        case String:
            oss << string_def_;
            break;
        case Button:
        default:
            assert(false);
            break;
        }

        return oss.str();
    }

    OptionType get_type() const { return type_; }

    bool        check_value()   { return check_value_; }
    int         spin_value()    { return spin_value_; }
    std::string combo_value()   { return combo_value_; }
    std::string string_value()  { return string_value_; }

    void set_value(std::string value)
    {
        switch (type_) {
        case Check:
            check_value_ = value == "true";
            break;
        case Spin:
            spin_value_ = std::stoi(value);
            break;
        case Combo:
            combo_value_ = value;
            break;
        case String:
            string_value_ = value;
            break;
        case Button:
        default:
            assert(type_ != Button);
            break;
        }
    }
    
    std::string get_value() const
    {
        switch (type_) {
        case Check:
            return check_value_ ? "true" : "false";
        case Spin:
            return std::to_string(spin_value_);
        case Combo:
            return combo_value_;
        case String:
            return string_value_;
        case Button:
        default:
            return std::string();
        }
    }

private:
    std::string name_;
    OptionType type_;

    // Check and spin
    
    int spin_min_;
    int spin_def_;
    int spin_max_;
    int spin_value_;

    bool check_def_;
    bool check_value_;

    // Combo and string

    std::string combo_def_;
    std::vector<std::string> combo_values_;
    std::string combo_value_;

    std::string string_def_;
    std::string string_value_;
};

class UCIOptionList {
public:
    UCIOption& get(std::string name)
    {
        for (UCIOption& opt : opts_)
            if (opt.name() == name)
                return opt;

        assert(false);
        return opts_[opts_.size()];
    }

    bool has_opt(std::string name) const
    {
        for (const UCIOption& opt : opts_)
            if (opt.name() == name)
                return true;

        return false;
    }

    void add(UCIOption opt)
    {
        assert(!has_opt(opt.name()));

        opts_.push_back(opt);
    }

    UCIOption& get(std::size_t i)
    {
        return opts_[i];
    }

    std::size_t count() const { return opts_.size(); }

private:
    std::vector<UCIOption> opts_;
};

#endif
